#pragma once
#include <stdbool.h>

enum { VIEW_COVERFLOW, VIEW_GRID, VIEW_LIST, VIEW_COUNT };

typedef struct {
    int theme;
    int lang;            /* 0 pt, 1 en, 2 es */
    int view;
    int sort;
    bool bg_video;
    bool game_music;
    bool menu_music;
    bool sfx;
    int volume;          /* 0..10 */
    bool clock24;
    int clock_offset;    /* minutes added to the console clock */
    int screensaver;     /* minutes, 0 = off */
    bool launch_anim;
    bool show_disc;      /* mini disc sliding out of the selected case */
    char last_game[8];
    int last_disc;
    bool resume;         /* at power on, a countdown then the last game */
} settings_t;

extern settings_t g_set;

void settings_defaults(void);
void settings_load(void);
bool settings_save(void);
