#include "memcard.h"
#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

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
            if (need && need <= 0x4000 && need <= dir.filelen && CARD_OpenEntry(slot, &dir, &f) >= 0) {
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

/* GCMCE / MemCard PRO GC: commands 0x8B xx on the card's own EXI line (protocol in the FlipperMCE
 * firmware, doc/mcp/memcardpro.txt). A plain memory card ignores them. */
static bool mce_xfer(int slot, uint8_t *out, int nout, uint8_t *in, int nin) {
    if (!inited) { CARD_Init("CMNU", "HB"); inited = true; }
    bool attached = EXI_Attach(slot, NULL) > 0;
    bool ok = false;
    if (EXI_Lock(slot, EXI_DEVICE_0, NULL) > 0) {
        if (EXI_Select(slot, EXI_DEVICE_0, EXI_SPEED16MHZ) > 0) {
            ok = EXI_ImmEx(slot, out, nout, EXI_WRITE) > 0;
            if (ok && nin) ok = EXI_ImmEx(slot, in, nin, EXI_READ) > 0;
            EXI_Deselect(slot);
        }
        EXI_Unlock(slot);
    }
    if (attached) EXI_Detach(slot);
    return ok;
}

bool memcard_is_mce(int slot) {
    uint8_t cmd[2] = {0x8B, 0x00}, r[4] = {0};
    return mce_xfer(slot, cmd, 2, r, 4) && r[0] == 0x38 && r[1] == 0x42;
}

/* "Set disc ID": GALE01 + disc and version as two ASCII digits each; the card opens that game's virtual card */
bool memcard_mce_set_game(int slot, const char *id6) {
    uint8_t cmd[12] = {0x8B, 0x11};
    memcpy(cmd + 2, id6, 6);
    memcpy(cmd + 8, "0000", 4);
    return mce_xfer(slot, cmd, sizeof cmd, NULL, 0);
}

/* Copies the saves (all, or those whose game code starts with code4) to dir as .gci files: the
 * 64-byte directory entry, then the file's blocks. Returns how many, -1 when the card cannot be read. */
int memcard_backup(int slot, const char *code4, const char *dir) {
    if (!inited) { CARD_Init("CMNU", "HB"); inited = true; }
    s32 mem = 0, sect = 0;
    if (CARD_ProbeEx(slot, &mem, &sect) < 0 || CARD_Mount(slot, workarea, NULL) < 0) return -1;
    mkdir(dir, 0777);
    int n = 0;
    card_dir d;
    uint8_t *buf = memalign(32, 8192);
    for (s32 r = CARD_FindFirst(slot, &d, true); r >= 0; r = CARD_FindNext(&d)) {
        if (code4 && strncmp((char *)d.gamecode, code4, 4)) continue;
        card_stat st;
        card_file f;
        char name[CARD_FILENAMELEN + 1];
        snprintf(name, sizeof name, "%.*s", CARD_FILENAMELEN, d.filename);
        if (CARD_GetStatus(slot, d.fileno, &st) < 0 || CARD_OpenEntry(slot, &d, &f) < 0) continue;
        uint8_t h[64];
        memset(h, 0, sizeof h);
        memcpy(h, d.gamecode, 4);
        memcpy(h + 4, d.company, 2);
        h[6] = 0xFF;
        h[7] = st.banner_fmt;
        memcpy(h + 8, d.filename, CARD_FILENAMELEN);
        uint16_t blocks = d.filelen / 8192;
        uint32_t be[] = {st.time, st.icon_addr};
        for (int i = 0; i < 2; i++) { h[0x28 + i * 4] = be[i] >> 24; h[0x29 + i * 4] = be[i] >> 16; h[0x2A + i * 4] = be[i] >> 8; h[0x2B + i * 4] = be[i]; }
        h[0x30] = st.icon_fmt >> 8; h[0x31] = st.icon_fmt;
        h[0x32] = st.icon_speed >> 8; h[0x33] = st.icon_speed;
        h[0x34] = d.permissions;
        h[0x36] = 0xFF; h[0x37] = 0xFF;          /* first block: meaningless off the card */
        h[0x38] = blocks >> 8; h[0x39] = blocks;
        h[0x3A] = 0xFF; h[0x3B] = 0xFF;
        h[0x3C] = st.comment_addr >> 24; h[0x3D] = st.comment_addr >> 16; h[0x3E] = st.comment_addr >> 8; h[0x3F] = st.comment_addr;
        char path[256], safe[CARD_FILENAMELEN + 1];
        for (int i = 0; i <= CARD_FILENAMELEN; i++) {
            char c = name[i];
            safe[i] = !c ? 0 : (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.') ? c : '_';
        }
        snprintf(path, sizeof path, "%s/%.4s%.2s-%s.gci", dir, (char *)d.gamecode, (char *)d.company, safe);
        FILE *out = fopen(path, "wb");
        bool ok = out && fwrite(h, 1, 64, out) == 64;
        for (uint32_t off = 0; ok && off < d.filelen; off += 8192) {
            ok = CARD_Read(&f, buf, 8192, off) >= 0;
            if (ok) { DCInvalidateRange(buf, 8192); ok = fwrite(buf, 1, 8192, out) == 8192; }
        }
        if (out) fclose(out);
        if (ok) n++;
        else remove(path);
        CARD_Close(&f);
    }
    free(buf);
    CARD_Unmount(slot);
    return n;
}
