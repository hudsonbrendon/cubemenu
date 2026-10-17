#include "memcard.h"
#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

static uint8_t workarea[CARD_WORKAREA] __attribute__((aligned(32)));
static bool inited;

void memcard_free(save_t *s, int n) {
    for (int i = 0; i < n; i++) tex_free(&s[i].icon);
}

/* save icons are 32x32, either RGB5A3 (already GX-tiled) or CI8 (8x4 tiles) + an RGB5A3 palette */
static void icon_from_ci8(tex_t *t, const uint8_t *ci, const uint8_t *tlut) {
    uint16_t lin[32 * 32];
    for (int ty = 0; ty < 32 / 4; ty++)
        for (int tx = 0; tx < 32 / 8; tx++)
            for (int y = 0; y < 4; y++)
                for (int x = 0; x < 8; x++) {
                    uint8_t idx = *ci++;
                    lin[(ty * 4 + y) * 32 + tx * 8 + x] = tlut[idx * 2] << 8 | tlut[idx * 2 + 1];
                }
    uint16_t *out = t->data;
    for (int ty = 0; ty < 32 / 4; ty++)
        for (int tx = 0; tx < 32 / 4; tx++)
            for (int y = 0; y < 4; y++)
                for (int x = 0; x < 4; x++) *out++ = lin[(ty * 4 + y) * 32 + tx * 4 + x];
}

static void ascii(char *dst, int n, const uint8_t *src, int len) {
    int k = 0;
    for (int i = 0; i < len && src[i] && k < n - 1; i++) dst[k++] = src[i] >= 32 && src[i] < 127 ? src[i] : ' ';
    while (k > 0 && dst[k - 1] == ' ') k--;
    dst[k] = 0;
}

int memcard_scan(int slot, save_t *out, int max, int *free_blocks) {
    if (!inited) { CARD_Init("CMNU", "HB"); inited = true; }
    *free_blocks = 0;
    s32 mem = 0, sect = 0;
    if (CARD_ProbeEx(slot, &mem, &sect) < 0) return -1;
    if (CARD_Mount(slot, workarea, NULL) < 0) return -2;
    int n = 0;
    uint32_t used = 0;
    card_dir dir;
    s32 r = CARD_FindFirst(slot, &dir, true);
    uint8_t *buf = memalign(32, 0x4000);
    while (r >= 0 && n < max) {
        save_t *s = &out[n];
        memset(s, 0, sizeof *s);
        snprintf(s->file, sizeof s->file, "%.*s", CARD_FILENAMELEN, dir.filename);
        snprintf(s->gamecode, sizeof s->gamecode, "%.4s%.2s", (char *)dir.gamecode, (char *)dir.company);
        s->blocks = dir.filelen / 8192;
        used += s->blocks;
        card_stat st;
        if (CARD_GetStatus(slot, dir.fileno, &st) >= 0) {
            s->time = st.time;
            /* read the head of the file: comment and first icon frame live there */
            uint32_t need = 0;
            if (st.comment_addr != 0xFFFFFFFF) need = st.comment_addr + 64;
            int ifmt = CARD_GetIconFmt(&st, 0);
            if (ifmt == CARD_ICON_RGB && st.offset_icon[0] + 2048 > need) need = st.offset_icon[0] + 2048;
            if (ifmt == CARD_ICON_CI) {
                if (st.offset_icon[0] + 1024 > need) need = st.offset_icon[0] + 1024;
                if (st.offset_icon_tlut[0] + 512 > need) need = st.offset_icon_tlut[0] + 512;
            }
            need = (need + 511) & ~511u;
            card_file f;
            if (need && need <= 0x4000 && need <= dir.filelen && CARD_Open(slot, s->file, &f) >= 0) {
                if (CARD_Read(&f, buf, need, 0) >= 0) {
                    DCInvalidateRange(buf, need);
                    if (st.comment_addr != 0xFFFFFFFF) {
                        ascii(s->title, sizeof s->title, buf + st.comment_addr, 32);
                        ascii(s->sub, sizeof s->sub, buf + st.comment_addr + 32, 32);
                    }
                    if (ifmt != CARD_ICON_NONE && tex_create(&s->icon, 32, 32, GX_TF_RGB5A3)) {
                        if (ifmt == CARD_ICON_RGB) memcpy(s->icon.data, buf + st.offset_icon[0], 2048);
                        else icon_from_ci8(&s->icon, buf + st.offset_icon[0], buf + st.offset_icon_tlut[0]);
                        tex_update(&s->icon);
                    }
                }
                CARD_Close(&f);
            }
        }
        if (!s->title[0]) snprintf(s->title, sizeof s->title, "%s", s->file);
        n++;
        r = CARD_FindNext(&dir);
    }
    free(buf);
    u16 blocks = 0;
    if (CARD_GetBlockCount(slot, &blocks) >= 0) *free_blocks = blocks > used + 5 ? blocks - 5 - used : 0;   /* 5 system blocks */
    CARD_Unmount(slot);
    return n;
}
