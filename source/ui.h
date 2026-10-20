#pragma once
#include "gfx.h"
#include "font.h"

extern tex_t g_tex_logo, g_tex_emblem, g_tex_wordmark;

void ui_init(void);
void ui_tick(float dt);                    /* global animation clock */
float ui_time(void);

/* background: theme gradient, slow floating cubes, optional full-screen clip */
void ui_background(float clip_alpha);
void ui_topbar(const char *title, const char *subtitle);
void ui_clock_string(char *buf, int n);

/* button glyphs in GameCube colours: A green, B red, X/Y grey, START grey, Z purple, L/R grey, C yellow */
enum { G_A, G_B, G_X, G_Y, G_START, G_Z, G_L, G_R, G_C, G_DPAD, G_LR };
float ui_glyph(int g, float x, float cy, float scale);    /* returns width */
float ui_glyph_width(int g);
/* bottom hint bar: pairs of (glyph, label), terminated by -1 */
void ui_hints(const int *glyphs, const char *const *labels, int n);

void ui_panel(float x, float y, float w, float h, float alpha);
void ui_toast(const char *msg);            /* shows for 2.5 s */
bool ui_toast_busy(void);
void ui_toast_draw(void);
void ui_scrollbar(float x, float y, float h, float pos, float frac);
