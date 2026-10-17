#pragma once
#include "gfx.h"

typedef struct {
    uint16_t x, y;
    uint8_t w, h;
    int8_t xo, yo;
    uint8_t adv, pad[3];
} glyph_t;

typedef struct {
    tex_t tex;
    int line, ascent;
    const glyph_t *g;    /* 224 entries, codepoints 32..255 */
} font_t;

extern font_t g_font_s, g_font_m, g_font_l, g_font_xl;

void font_init(void);
float text_width(const font_t *f, const char *s);
float text_width_n(const font_t *f, const char *s, int bytes);
/* top-left anchored; returns the advance */
float draw_text(const font_t *f, float x, float y, uint32_t c, const char *s);
void draw_text_center(const font_t *f, float cx, float y, uint32_t c, const char *s);
void draw_text_right(const font_t *f, float rx, float y, uint32_t c, const char *s);
void draw_text_shadow(const font_t *f, float x, float y, uint32_t c, const char *s);
/* clipped to max width with an ellipsis */
void draw_text_fit(const font_t *f, float x, float y, float maxw, uint32_t c, const char *s);
/* word-wrapped paragraph; skips 'skip_lines' lines, draws at most max_lines; returns total lines */
int draw_text_wrap(const font_t *f, float x, float y, float w, float line_h, int skip_lines, int max_lines, uint32_t c, const char *s);
