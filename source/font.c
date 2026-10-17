#include "font.h"
#include <string.h>
#include "font_s_bin.h"
#include "font_m_bin.h"
#include "font_l_bin.h"
#include "font_xl_bin.h"

font_t g_font_s, g_font_m, g_font_l, g_font_xl;

static void load(font_t *f, const uint8_t *b, int len) {
    f->line = b[4] << 8 | b[5];
    f->ascent = b[6] << 8 | b[7];
    int w = b[8] << 8 | b[9], h = b[10] << 8 | b[11];
    f->g = (const glyph_t *)(b + 16);
    int off = 16 + 224 * 12;
    off = (off + 31) & ~31;
    /* wrap the atlas as a .gxt-less texture */
    memset(&f->tex, 0, sizeof f->tex);
    f->tex.w = w; f->tex.h = h; f->tex.fmt = GX_TF_IA8;
    f->tex.data = (void *)(b + off);
    f->tex.borrowed = 1;
    (void)len;
    tex_update(&f->tex);
}

void font_init(void) {
    load(&g_font_s, font_s_bin, font_s_bin_size);
    load(&g_font_m, font_m_bin, font_m_bin_size);
    load(&g_font_l, font_l_bin, font_l_bin_size);
    load(&g_font_xl, font_xl_bin, font_xl_bin_size);
}

/* UTF-8 -> Latin-1 codepoint (others become '?') */
static int next_cp(const char **ps) {
    const unsigned char *s = (const unsigned char *)*ps;
    int c = *s++;
    if (c >= 0x80) {
        if ((c & 0xE0) == 0xC0 && (*s & 0xC0) == 0x80) { c = ((c & 0x1F) << 6) | (*s++ & 0x3F); }
        else { while ((*s & 0xC0) == 0x80) s++; c = '?'; }
        if (c > 255) c = '?';
    }
    *ps = (const char *)s;
    return c;
}

static inline const glyph_t *glyph(const font_t *f, int cp) {
    if (cp < 32 || cp > 255) cp = '?';
    return &f->g[cp - 32];
}

float text_width_n(const font_t *f, const char *s, int bytes) {
    float w = 0;
    const char *end = s + bytes;
    while (*s && s < end) w += glyph(f, next_cp(&s))->adv;
    return w;
}

float text_width(const font_t *f, const char *s) { return text_width_n(f, s, strlen(s)); }

static float draw_n(const font_t *f, float x, float y, uint32_t c, const char *s, int bytes) {
    float x0 = x;
    const char *end = s + bytes;
    float iw = 1.0f / f->tex.w, ih = 1.0f / f->tex.h;
    x = (int)x; y = (int)y;
    while (*s && s < end) {
        const glyph_t *g = glyph(f, next_cp(&s));
        if (g->w) {
            draw_tex_uv(&f->tex, x + g->xo, y + g->yo, g->w, g->h,
                        g->x * iw, g->y * ih, (g->x + g->w) * iw, (g->y + g->h) * ih, c);
        }
        x += g->adv;
    }
    return x - x0;
}

float draw_text(const font_t *f, float x, float y, uint32_t c, const char *s) { return draw_n(f, x, y, c, s, strlen(s)); }

void draw_text_center(const font_t *f, float cx, float y, uint32_t c, const char *s) {
    draw_text(f, cx - text_width(f, s) / 2, y, c, s);
}

void draw_text_right(const font_t *f, float rx, float y, uint32_t c, const char *s) {
    draw_text(f, rx - text_width(f, s), y, c, s);
}

void draw_text_shadow(const font_t *f, float x, float y, uint32_t c, const char *s) {
    draw_text(f, x + 2, y + 2, col_alpha(0x00000000 | 0xFF, (c & 0xFF) / 255.0f * 0.55f), s);
    draw_text(f, x, y, c, s);
}

void draw_text_fit(const font_t *f, float x, float y, float maxw, uint32_t c, const char *s) {
    if (text_width(f, s) <= maxw) { draw_text(f, x, y, c, s); return; }
    float ell = text_width(f, "...");
    int n = strlen(s);
    while (n > 0 && text_width_n(f, s, n) + ell > maxw) {
        n--;
        while (n > 0 && (s[n] & 0xC0) == 0x80) n--;
    }
    float w = draw_n(f, x, y, c, s, n);
    draw_text(f, x + w, y, c, "...");
}

int draw_text_wrap(const font_t *f, float x, float y, float w, float line_h, int skip, int max_lines, uint32_t c, const char *s) {
    int line = 0;
    while (*s) {
        if (*s == '\n') { s++; line++; continue; }
        /* longest run of words that fits */
        const char *p = s, *brk = NULL;
        while (*p && *p != '\n') {
            const char *q = p;
            while (*q && *q != ' ' && *q != '\n') q++;
            if (text_width_n(f, s, q - s) > w) break;
            brk = q;
            p = q;
            if (*p == ' ') p++;
        }
        if (!brk) {   /* one word longer than the line: hard break */
            brk = s;
            while (*brk && *brk != '\n' && text_width_n(f, s, brk - s + 1) <= w) brk++;
            if (brk == s) brk++;
        }
        if (line >= skip && line - skip < max_lines) draw_n(f, x, y + (line - skip) * line_h, c, s, brk - s);
        line++;
        s = brk;
        while (*s == ' ') s++;
        if (*s == '\n') s++;
    }
    return line;
}
