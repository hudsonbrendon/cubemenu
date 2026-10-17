#include "trace.h"
#include "font.h"
#include "fs.h"
#include "gfx.h"
#include "ui.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifndef CUBEMENU_VERSION
#define CUBEMENU_VERSION "dev"
#endif

static char lines[12][96];
static int n;
static bool card, done, video;

static void log_line(const char *s, const char *mode) {
    char path[96];
    FILE *f = fopen(fs_path(path, sizeof path, "/cubemenu/boot.log"), mode);
    if (!f) return;
    fprintf(f, "%s\n", s);
    fclose(f);
}

static void draw(void) {
    for (int k = 0; k < 2; k++) {
        gfx_begin(0x140F33FF);
        draw_tex(&g_tex_emblem, 40, 36, 48, 48, 0xFFFFFFFF);
        draw_text(&g_font_m, 100, 48, 0xFFFFFFFF, "Cube Menu " CUBEMENU_VERSION);
        int first = n > 12 ? n - 12 : 0;
        for (int i = first; i < n; i++)
            draw_text(&g_font_s, 40, 110 + (i - first) * 24, i == n - 1 ? 0xFFD040FF : 0xB8B0E8FF, lines[i % 12]);
        gfx_end();
    }
}

void trace(const char *fmt, ...) {
    if (done) return;
    char buf[96];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    snprintf(lines[n % 12], sizeof lines[0], "%s", buf);
    n++;
    if (card && fs_writable()) log_line(buf, "a");
    if (video) draw();
}

void trace_card_ready(void) {
    card = true;
    if (!fs_writable()) return;
    log_line("Cube Menu " CUBEMENU_VERSION " start-up", "w");
    int first = n > 12 ? n - 12 : 0;
    for (int i = first; i < n; i++) log_line(lines[i % 12], "a");
}

void trace_video_ready(void) { video = true; draw(); }

void trace_done(void) {
    if (card && fs_writable()) log_line("menu up", "a");
    done = true;
}
