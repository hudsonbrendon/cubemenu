#include "ui.h"
#include "theme.h"
#include "settings.h"
#include "clip.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "logo_bin.h"
#include "emblem_bin.h"
#include "wordmark_bin.h"

tex_t g_tex_logo, g_tex_emblem, g_tex_wordmark;
static float t_now;
static char toast_msg[128];
static float toast_t = -10;

void ui_init(void) {
    tex_from_gxt(&g_tex_logo, logo_bin, logo_bin_size, false);
    tex_from_gxt(&g_tex_emblem, emblem_bin, emblem_bin_size, false);
    tex_from_gxt(&g_tex_wordmark, wordmark_bin, wordmark_bin_size, false);
}

void ui_tick(float dt) { t_now += dt; }
float ui_time(void) { return t_now; }

/* a flat iso cube outline, the menu's background motif */
static void line(float x0, float y0, float x1, float y1, float w, uint32_t c) {
    float dx = x1 - x0, dy = y1 - y0, l = sqrtf(dx * dx + dy * dy);
    if (l < 0.01f) return;
    float nx = -dy / l * w, ny = dx / l * w;
    float q[8] = {x0 - nx, y0 - ny, x1 - nx, y1 - ny, x1 + nx, y1 + ny, x0 + nx, y0 + ny};
    uint32_t cs[4] = {c, c, c, c};
    draw_quad_flat(q, cs);
}

static void bg_cube(float cx, float cy, float s, float rot, uint32_t c) {
    float px[6], py[6];
    for (int i = 0; i < 6; i++) {
        float a = rot + i * M_PI / 3 - M_PI / 2;
        px[i] = cx + cosf(a) * s; py[i] = cy + sinf(a) * s;
    }
    for (int i = 0; i < 6; i++) line(px[i], py[i], px[(i + 1) % 6], py[(i + 1) % 6], 1.2f, c);
    for (int i = 1; i < 6; i += 2) line(cx, cy, px[i], py[i], 1.2f, c);
}

void ui_background(float clip_alpha) {
    draw_rect_grad(0, 0, SCREEN_W, SCREEN_H, th->bg_top, th->bg_bottom);
    /* drifting cube outlines */
    for (int i = 0; i < 7; i++) {
        float sp = 0.012f + 0.006f * (i % 3);
        float x = fmodf(90 + i * 137.0f + t_now * 18 * sp * 60, SCREEN_W + 160) - 80;
        float y = 70 + fmodf(i * 97.0f, 360) + sinf(t_now * 0.4f + i) * 14;
        float s = 26 + (i * 13) % 34;
        bg_cube(x, y, s, t_now * 0.05f * ((i & 1) ? 1 : -1), col_alpha(th->text_hi, 0.045f + 0.02f * (i % 3)));
    }
    const tex_t *f = clip_frame();
    if (f && clip_alpha > 0.01f) {
        float a = clip_alpha * clampf(clip_age() / 0.5f, 0, 1);
        draw_tex(f, 0, 0, SCREEN_W, SCREEN_H, col_alpha(0xFFFFFFFF, a));
        /* keep text readable: darken + theme tint, stronger at the bottom */
        draw_rect_grad(0, 0, SCREEN_W, SCREEN_H, col_alpha(th->bg_top, 0.55f * a), col_alpha(th->bg_top, 0.86f * a));
    }
    /* vignette */
    draw_rect_grad(0, 0, SCREEN_W, 90, 0x00000070, 0x00000000);
    draw_rect_grad(0, SCREEN_H - 110, SCREEN_W, 110, 0x00000000, 0x00000090);
}

void ui_clock_string(char *buf, int n) {
    time_t now = time(NULL) + g_set.clock_offset * 60;
    struct tm *tm = gmtime(&now);
    if (g_set.clock24) snprintf(buf, n, "%02d:%02d", tm->tm_hour, tm->tm_min);
    else snprintf(buf, n, "%d:%02d %s", (tm->tm_hour + 11) % 12 + 1, tm->tm_min, tm->tm_hour < 12 ? "AM" : "PM");
}

void ui_topbar(const char *title, const char *subtitle) {
    draw_tex(&g_tex_emblem, 34, 22, 40, 40, 0xFFFFFFFF);
    draw_tex(&g_tex_wordmark, 80, 30, 128, 20, col_alpha(0xFFFFFFFF, 0.92f));
    if (title) {
        float x = 230;
        draw_rect(x - 12, 30, 2, 22, col_alpha(th->text_dim, 0.6f));
        draw_text(&g_font_m, x, 27, th->text_hi, title);
        float sx = x + text_width(&g_font_m, title) + 12;
        if (subtitle && sx + text_width(&g_font_s, subtitle) < SCREEN_W - 120) draw_text(&g_font_s, sx, 31, th->text_dim, subtitle);
    }
    char clk[16];
    ui_clock_string(clk, sizeof clk);
    draw_text_right(&g_font_m, SCREEN_W - 36, 27, th->text_hi, clk);
}

/* ---------------- glyphs ---------------- */

float ui_glyph_width(int g) {
    switch (g) {
    case G_A: return 26;
    case G_B: return 21;
    case G_X: case G_Y: case G_Z: case G_L: case G_R: return 30;
    case G_START: return 54;
    case G_LR: return 44;
    case G_C: return 22;
    case G_DPAD: return 22;
    }
    return 0;
}

static void pill(float x, float cy, float w, float h, uint32_t c) { draw_round_rect(x, cy - h / 2, w, h, h / 2, c); }

float ui_glyph(int g, float x, float cy, float k) {
    const uint32_t GREEN = 0x2DB84BFF, RED = 0xE2343AFF, GREY = 0xB9B9C2FF, PURPLE = 0x6C5BD6FF, YELLOW = 0xF4C21AFF;
    const uint32_t INK = 0xFFFFFFFF, DARK = 0x2A2A30FF;
    float r;
    switch (g) {
    case G_A:
        r = 13 * k;
        draw_circle(x + r, cy + 1.5f, r, 0x00000060);
        draw_circle(x + r, cy, r, GREEN);
        draw_text_center(&g_font_s, x + r, cy - g_font_s.ascent * 0.62f, INK, "A");
        return 2 * r;
    case G_B:
        r = 10.5f * k;
        draw_circle(x + r, cy + 1.5f, r, 0x00000060);
        draw_circle(x + r, cy, r, RED);
        draw_text_center(&g_font_s, x + r, cy - g_font_s.ascent * 0.62f, INK, "B");
        return 2 * r;
    case G_X: case G_Y: {
        /* kidney-shaped grey buttons: a tilted pill */
        float w = 30 * k, h = 18 * k;
        pill(x, cy + 1.5f, w, h, 0x00000060);
        pill(x, cy, w, h, GREY);
        draw_text_center(&g_font_s, x + w / 2, cy - g_font_s.ascent * 0.62f, DARK, g == G_X ? "X" : "Y");
        return w;
    }
    case G_START: {
        float w = 54 * k, h = 18 * k;
        pill(x, cy, w, h, GREY);
        draw_text_center(&g_font_s, x + w / 2, cy - g_font_s.ascent * 0.62f, DARK, "START");
        return w;
    }
    case G_Z: {
        float w = 30 * k, h = 17 * k;
        pill(x, cy, w, h, PURPLE);
        draw_text_center(&g_font_s, x + w / 2, cy - g_font_s.ascent * 0.62f, INK, "Z");
        return w;
    }
    case G_L: case G_R: case G_LR: {
        const char *s = g == G_L ? "L" : g == G_R ? "R" : "L/R";
        float w = (g == G_LR ? 44 : 28) * k, h = 18 * k;
        draw_round_rect(x, cy - h / 2, w, h, 4 * k, GREY);
        draw_text_center(&g_font_s, x + w / 2, cy - g_font_s.ascent * 0.62f, DARK, s);
        return w;
    }
    case G_C:
        r = 11 * k;
        draw_circle(x + r, cy, r, YELLOW);
        draw_text_center(&g_font_s, x + r, cy - g_font_s.ascent * 0.62f, DARK, "C");
        return 2 * r;
    case G_DPAD: {
        float s = 22 * k, t = 7 * k;
        draw_rect(x + (s - t) / 2, cy - s / 2, t, s, GREY);
        draw_rect(x, cy - t / 2, s, t, GREY);
        return s;
    }
    }
    return 0;
}

void ui_hints(const int *glyphs, const char *const *labels, int n) {
    float y = SCREEN_H - 38;
    /* measure, then centre */
    float gap = 20, total;
    for (;;) {
        total = 0;
        for (int i = 0; i < n; i++) total += ui_glyph_width(glyphs[i]) + 6 + text_width(&g_font_s, labels[i]) + (i < n - 1 ? gap : 0);
        if (total <= SCREEN_W - 60 || gap <= 10) break;
        gap -= 2;
    }
    while (total > SCREEN_W - 60 && n > 1) {     /* still too wide: drop the last hint */
        n--;
        total = 0;
        for (int i = 0; i < n; i++) total += ui_glyph_width(glyphs[i]) + 6 + text_width(&g_font_s, labels[i]) + (i < n - 1 ? gap : 0);
    }
    float x = (SCREEN_W - total) / 2;
    for (int i = 0; i < n; i++) {
        float w = ui_glyph(glyphs[i], x, y, 1.0f);
        x += w + 6;
        draw_text(&g_font_s, x, y - g_font_s.ascent * 0.62f, th->text, labels[i]);
        x += text_width(&g_font_s, labels[i]) + gap;
    }
}

void ui_panel(float x, float y, float w, float h, float a) {
    draw_round_rect(x, y, w, h, 14, col_alpha(th->panel, a * ((th->panel & 0xFF) / 255.0f) * 2.2f));
    draw_round_rect(x, y, w, 2, 1, col_alpha(th->text_hi, 0.10f * a));
}

void ui_toast(const char *msg) {
    snprintf(toast_msg, sizeof toast_msg, "%s", msg);
    toast_t = t_now;
}

void ui_toast_draw(void) {
    float age = t_now - toast_t;
    if (age > 2.8f || !toast_msg[0]) return;
    float a = clampf(age / 0.2f, 0, 1) * clampf((2.8f - age) / 0.4f, 0, 1);
    float w = text_width(&g_font_s, toast_msg) + 40, x = (SCREEN_W - w) / 2, y = SCREEN_H - 96 + (1 - a) * 12;
    draw_round_rect(x, y, w, 34, 17, col_alpha(0x000000E0, a * 0.85f));
    draw_round_rect(x, y, w, 34, 17, col_alpha(th->accent, a * 0.18f));
    draw_text_center(&g_font_s, SCREEN_W / 2, y + 7, col_alpha(0xFFFFFFFF, a), toast_msg);
}

void ui_scrollbar(float x, float y, float h, float pos, float frac) {
    if (frac >= 1) return;
    draw_round_rect(x, y, 4, h, 2, col_alpha(th->text_hi, 0.12f));
    float bh = fmaxf(20, h * frac);
    draw_round_rect(x, y + (h - bh) * clampf(pos, 0, 1), 4, bh, 2, col_alpha(th->accent, 0.9f));
}

bool ui_toast_busy(void) { return t_now - toast_t < 3.0f; }
