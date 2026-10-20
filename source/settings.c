#include "settings.h"
#include "fs.h"
#include "ini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

settings_t g_set;

void settings_defaults(void) {
    memset(&g_set, 0, sizeof g_set);
    g_set.lang = 0;
    g_set.view = VIEW_COVERFLOW;
    g_set.bg_video = true;
    g_set.game_music = true;
    g_set.menu_music = true;
    g_set.sfx = true;
    g_set.volume = 8;
    g_set.clock24 = true;
    g_set.screensaver = 5;
    g_set.launch_anim = true;
    g_set.show_disc = true;
}

#define CLAMPI(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))

void settings_load(void) {
    settings_defaults();
    char path[96];
    char *txt = fs_read_all(fs_path(path, sizeof path, "/cubemenu/settings.ini"), NULL);
    if (!txt) return;
    ini_t ini;
    ini_parse(&ini, txt);
    g_set.theme = ini_get_int(&ini, "theme", g_set.theme);
    g_set.lang = CLAMPI(ini_get_int(&ini, "language", g_set.lang), 0, 2);
    g_set.view = CLAMPI(ini_get_int(&ini, "view", g_set.view), 0, VIEW_COUNT - 1);
    g_set.sort = CLAMPI(ini_get_int(&ini, "sort", g_set.sort), 0, 3);
    g_set.bg_video = ini_get_int(&ini, "background_video", g_set.bg_video);
    g_set.game_music = ini_get_int(&ini, "game_music", g_set.game_music);
    g_set.menu_music = ini_get_int(&ini, "menu_music", g_set.menu_music);
    g_set.sfx = ini_get_int(&ini, "ui_sounds", g_set.sfx);
    g_set.volume = CLAMPI(ini_get_int(&ini, "volume", g_set.volume), 0, 10);
    g_set.clock24 = ini_get_int(&ini, "clock_24h", g_set.clock24);
    g_set.clock_offset = CLAMPI(ini_get_int(&ini, "clock_offset", 0), -24 * 60, 24 * 60);
    g_set.screensaver = CLAMPI(ini_get_int(&ini, "screensaver", g_set.screensaver), 0, 60);
    g_set.launch_anim = ini_get_int(&ini, "launch_animation", g_set.launch_anim);
    g_set.show_disc = ini_get_int(&ini, "show_disc", g_set.show_disc);
    g_set.resume = ini_get_int(&ini, "resume", 0);
    g_set.last_disc = CLAMPI(ini_get_int(&ini, "last_disc", 0), 0, 3);
    const char *v = ini_get(&ini, "last_game");
    if (v) snprintf(g_set.last_game, sizeof g_set.last_game, "%s", v);
    free(txt);
}

bool settings_save(void) {
    char buf[1024];
    int n = snprintf(buf, sizeof buf,
        "# Cube Menu settings (written by the menu)\n"
        "theme=%d\nlanguage=%d\nview=%d\nsort=%d\nbackground_video=%d\ngame_music=%d\nmenu_music=%d\n"
        "ui_sounds=%d\nvolume=%d\nclock_24h=%d\nclock_offset=%d\nscreensaver=%d\nlaunch_animation=%d\n"
        "show_disc=%d\nlast_game=%s\nlast_disc=%d\nresume=%d\n",
        g_set.theme, g_set.lang, g_set.view, g_set.sort, g_set.bg_video, g_set.game_music, g_set.menu_music,
        g_set.sfx, g_set.volume, g_set.clock24, g_set.clock_offset, g_set.screensaver, g_set.launch_anim,
        g_set.show_disc, g_set.last_game, g_set.last_disc, g_set.resume);
    char path[96];
    return fs_write_all(fs_path(path, sizeof path, "/cubemenu/settings.ini"), buf, n);
}
