#include "clip.h"
#include <ogc/lwp.h>
#include <ogc/lwp_watchdog.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define NBUF 3
static tex_t bufs[NBUF];
static int bw, bh, fsize;
static volatile int shown = -1;
static volatile bool running;
static volatile int generation;
static char want[160];
static volatile bool want_new;
static u64 first_tick;
static lwp_t thread;
static uint8_t stack[32 * 1024] __attribute__((aligned(32)));

static void *worker(void *arg) {
    (void)arg;
    FILE *f = NULL;
    int nframes = 0, fps = 20, frame = 0, wbuf = 0;
    u64 t0 = 0;
    while (running) {
        if (want_new) {
            want_new = false;
            if (f) { fclose(f); f = NULL; }
            shown = -1;
            if (want[0] && (f = fopen(want, "rb"))) {
                uint8_t h[32];
                int w = 0, hh = 0;
                if (fread(h, 1, 32, f) == 32 && !memcmp(h, "CMV1", 4)) {
                    w = h[4] << 8 | h[5]; hh = h[6] << 8 | h[7];
                    fps = h[8] << 8 | h[9]; nframes = h[10] << 8 | h[11];
                }
                if (w != bw || hh != bh || !nframes || fps <= 0) { fclose(f); f = NULL; }
                frame = 0;
                t0 = gettime();
            }
        }
        if (!f) { usleep(10000); continue; }
        int due = (int)(ticks_to_millisecs(diff_ticks(t0, gettime())) * fps / 1000);
        if (due < frame) { usleep(4000); continue; }
        if (frame >= nframes) { fseek(f, 32, SEEK_SET); frame = 0; t0 = gettime(); continue; }
        if (due > frame + 2) {                        /* fell behind (slow card): skip ahead */
            frame = due < nframes ? due : nframes - 1;
            fseek(f, 32 + (long)frame * fsize, SEEK_SET);
        }
        wbuf = (shown + 1 + (shown < 0)) % NBUF;
        if (wbuf == shown) wbuf = (wbuf + 1) % NBUF;
        if (fread(bufs[wbuf].data, 1, fsize, f) != (size_t)fsize) { fseek(f, 32, SEEK_SET); frame = 0; t0 = gettime(); continue; }
        DCFlushRange(bufs[wbuf].data, fsize);
        gfx_invalidate();
        if (shown < 0) first_tick = gettime();
        shown = wbuf;
        frame++;
    }
    if (f) fclose(f);
    return NULL;
}

void clip_init(void) {
    if (running) return;
    if (!bufs[0].data) {
        bw = 320; bh = 176; fsize = bw * bh / 2;
        for (int i = 0; i < NBUF; i++) tex_create(&bufs[i], bw, bh, GX_TF_CMPR);
    }
    want[0] = 0;
    shown = -1;
    running = true;
    LWP_CreateThread(&thread, worker, NULL, stack, sizeof stack, 50);
}

void clip_play(const char *path) {
    if (path && !strcmp(path, want) ) return;
    snprintf(want, sizeof want, "%s", path ? path : "");
    shown = -1;
    want_new = true;
}

const tex_t *clip_frame(void) {
    int s = shown;
    return s >= 0 ? &bufs[s] : NULL;
}

float clip_age(void) {
    return shown >= 0 ? ticks_to_millisecs(diff_ticks(first_tick, gettime())) / 1000.0f : 0;
}

void clip_stop(void) {
    if (!running) return;
    running = false;
    LWP_JoinThread(thread, NULL);
}
