#include "assets.h"
#include "fs.h"
#include "library.h"
#include <ogc/lwp.h>
#include <ogc/mutex.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define KEEP 40          /* games with textures resident */

typedef struct {
    tex_t t[AS_COUNT];
    volatile uint8_t state[AS_COUNT];   /* 0 none, 1 ready, 2 missing */
} entry_t;

static const char *FILES[AS_COUNT] = {"case.gxt", "disc.gxt", "snap.gxt", "title.gxt"};
static entry_t ents[MAX_GAMES];
static int order[MAX_GAMES], norder, center, radius_;
static volatile int shots_game = -1;
static volatile bool running, dirty;
static lwp_t thread;
static mutex_t lock;
static uint8_t stack[64 * 1024] __attribute__((aligned(32)));

const tex_t *assets_get(int g, int k) { return g >= 0 && ents[g].state[k] == 1 ? &ents[g].t[k] : NULL; }
bool assets_missing(int g, int k) { return g >= 0 && ents[g].state[k] == 2; }

void assets_focus(const int *list, int n, int focus, int radius) {
    LWP_MutexLock(lock);
    if (n != norder || focus != center || memcmp(order, list, n * sizeof *list)) {
        memcpy(order, list, n * sizeof *list);
        norder = n; center = focus; radius_ = radius;
        dirty = true;
    }
    LWP_MutexUnlock(lock);
}

void assets_want_shots(int g) { shots_game = g; dirty = true; }

static bool load_one(int g, int k) {
    char rel[96], path[160];
    game_asset_path(rel, sizeof rel, &g_games[g], FILES[k]);
    tex_t t;
    if (tex_load(&t, fs_path(path, sizeof path, rel))) {
        ents[g].t[k] = t;
        ents[g].state[k] = 1;
    } else ents[g].state[k] = 2;
    return true;
}

static int distance(int pos, int c) { return pos > c ? pos - c : c - pos; }

static void *worker(void *arg) {
    (void)arg;
    while (running) {
        int list[MAX_GAMES], n, c, r;
        LWP_MutexLock(lock);
        n = norder; c = center; r = radius_;
        memcpy(list, order, n * sizeof *list);
        dirty = false;
        LWP_MutexUnlock(lock);
        bool did = false;
        /* screenshots for the details screen first */
        int sg = shots_game;
        if (sg >= 0) {
            for (int k = AS_SNAP; k <= AS_TITLE && !did; k++)
                if (!ents[sg].state[k]) did = load_one(sg, k);
        }
        /* then cases and discs, nearest first */
        for (int d = 0; d <= r && !did && running && !dirty; d++) {
            for (int s = -1; s <= 1 && !did; s += 2) {
                int pos = c + d * s;
                if (d == 0 && s == 1) continue;
                if (pos < 0 || pos >= n) continue;
                int g = list[pos];
                for (int k = AS_CASE; k <= AS_DISC && !did; k++)
                    if (!ents[g].state[k]) did = load_one(g, k);
            }
        }
        /* evict what is far away */
        if (!did) {
            int resident = 0;
            for (int i = 0; i < n; i++) if (ents[list[i]].state[AS_CASE] == 1) resident++;
            for (int i = 0; i < n && resident > KEEP; i++) {
                int pos = (i & 1) ? n - 1 - i / 2 : i / 2;      /* both ends of the list first */
                int g = list[pos];
                if (distance(pos, c) <= r + 4 || ents[g].state[AS_CASE] != 1) continue;
                ents[g].state[AS_CASE] = 0; ents[g].state[AS_DISC] = 0;
                tex_free(&ents[g].t[AS_CASE]); tex_free(&ents[g].t[AS_DISC]);
                resident--;
            }
            for (int g = 0; g < g_ngames; g++) {
                if (g == sg) continue;
                for (int k = AS_SNAP; k <= AS_TITLE; k++)
                    if (ents[g].state[k] == 1) { ents[g].state[k] = 0; tex_free(&ents[g].t[k]); }
            }
            usleep(16000);
        }
    }
    return NULL;
}

void assets_init(void) {
    LWP_MutexInit(&lock, false);
    running = true;
    LWP_CreateThread(&thread, worker, NULL, stack, sizeof stack, 40);
}

void assets_stop(void) {
    if (!running) return;
    running = false;
    LWP_JoinThread(thread, NULL);
}
