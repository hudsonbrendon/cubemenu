/* Cube Menu - a game menu for the Nintendo GameCube that launches games through Swiss. */
#include <gccore.h>
#include <ogc/lwp.h>
#include <ogc/lwp_watchdog.h>
#include "app.h"
#include "font.h"
#include "gfx.h"
#include "input.h"
#include "ui.h"

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    /* the UI thread outranks the loaders (40/50): they only run while it waits for vsync */
    LWP_SetThreadPriority(LWP_GetSelf(), 70);
    gfx_init();
    font_init();
    ui_init();
    input_init();
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
