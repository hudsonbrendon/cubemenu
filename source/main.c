/* Cube Menu - a game menu for the Nintendo GameCube that launches games through Swiss. */
#include <gccore.h>
#include <ogc/lwp.h>
#include <ogc/lwp_watchdog.h>
#include "app.h"
#include "font.h"
#include "gfx.h"
#include "input.h"
#include "ui.h"
#include "trace.h"
#include "fs.h"
#include <stdbool.h>

int main(int argc, char **argv) {
    (void)argv;
    /* the UI thread outranks the loaders (40/50): they only run while it waits for vsync */
    LWP_SetThreadPriority(LWP_GetSelf(), 70);
    trace("main: start (argc %d)", argc);
    bool card = fs_init();                 /* first, so the log works even if video does not */
    if (card) trace_card_ready();
    trace("card: %s (%s)", card ? fs_root() : "none", fs_label());
    trace("video: init...");
    gfx_init();
    trace("video: mode %u, %ux%u", gfx_mode()->viTVMode, gfx_mode()->fbWidth, gfx_mode()->efbHeight);
    font_init();
    ui_init();
    trace_video_ready();
    input_init();
    trace("controllers: ok");
    app_init();
    u64 last = gettime();
    while (1) {
        u64 now = gettime();
        float dt = ticks_to_microsecs(diff_ticks(last, now)) / 1e6f;
        last = now;
        if (dt > 0.1f) dt = 0.1f;
        app_frame(dt);
    }
    return 0;
}
