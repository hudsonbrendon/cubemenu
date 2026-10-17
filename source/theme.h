#pragma once
#include <stdint.h>

typedef struct {
    const char *name;        /* shown in settings */
    uint32_t console;        /* body colour of the GameCube in the launch scene */
    uint32_t bg_top, bg_bottom;
    uint32_t accent, accent2;
    uint32_t text_hi, text, text_dim;
    uint32_t panel;          /* translucent panels */
    uint32_t floor;          /* reflection floor tint */
} theme_t;

#define THEME_COUNT 6
extern const theme_t g_themes[THEME_COUNT];
extern const theme_t *th;    /* current */
void theme_set(int i);
