#include "app.h"
#include "assets.h"
#include "audio.h"
#include "boot.h"
#include "clip.h"
#include "font.h"
#include "fs.h"
#include "gfx.h"
#include "input.h"
#include "lang.h"
#include "memcard.h"
#include "library.h"
#include "scene3d.h"
#include "settings.h"
#include "theme.h"
#include "ui.h"
#include "trace.h"
#include <ctype.h>
#include <malloc.h>
#include <math.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <ogc/lwp_watchdog.h>

#ifndef CUBEMENU_VERSION
#define CUBEMENU_VERSION "dev"
#endif

enum { I_H_VISUAL, I_THEME, I_LANG, I_VIEW, I_BGVIDEO, I_SHOWDISC, I_LAUNCHANIM,
       I_H_AUDIO, I_GAMEMUSIC, I_MENUMUSIC, I_SFX, I_VOLUME,
       I_H_LIBRARY, I_SORT,
       I_H_SYSTEM, I_CLOCK24, I_CLOCKADJ, I_SAVER, I_OPENSWISS, I_RESET, I_COUNT };

static int screen = SCR_INTRO;
static const char *pending_toast;
static void mc_load(int slot);
static void mc_open_game(int gi);
static int mc_game = -1;          /* memory cards screen: >= 0 shows only that game's saves */
static float screen_t;
static int view[MAX_GAMES], nview;
static int filter = FILTER_ALL;
static int cur;
static float pos_s, sel_t;
static bool media_started;
static bool sidebar;
static float sidebar_a;
static int side_list[10], nside, side_cur;
static int disc_game = -1, disc_cur;
static float details_scroll;
static int details_lines, details_shot;
static int set_cur = 1;
static float set_scroll;
static int launch_game, launch_disc;
static float launch_t;
static bool saver;
static float saver_t;
static bool card_ok;

/* ---------------- demo / tour scripts ---------------- */

typedef struct { int frame; uint32_t buttons; } demo_ev;
static demo_ev *demo;
static int ndemo, demo_i, demo_frame;
static bool demo_fps;

static uint32_t button_by_name(const char *s) {
    static const struct { const char *n; uint32_t b; } T[] = {
        {"A", BTN_A}, {"B", BTN_B}, {"X", BTN_X}, {"Y", BTN_Y}, {"START", BTN_START}, {"Z", BTN_Z},
        {"L", BTN_L}, {"R", BTN_R}, {"UP", BTN_UP}, {"DOWN", BTN_DOWN}, {"LEFT", BTN_LEFT}, {"RIGHT", BTN_RIGHT},
        {"CLEFT", BTN_CLEFT}, {"CRIGHT", BTN_CRIGHT}, {"CUP", BTN_CUP}, {"CDOWN", BTN_CDOWN}};
    for (unsigned i = 0; i < sizeof T / sizeof *T; i++) if (!strcasecmp(s, T[i].n)) return T[i].b;
    return 0;
}

static void demo_load(void) {
    char path[96];
    char *txt = fs_read_all(fs_path(path, sizeof path, "/cubemenu/demo.txt"), NULL);
    if (!txt) return;
    demo = calloc(512, sizeof *demo);
    int frame = 0;
    for (char *line = strtok(txt, "\n"); line && ndemo < 512; line = strtok(NULL, "\n")) {
        if (!strncmp(line, "#fps", 4)) demo_fps = true;
        if (line[0] == '#') continue;
        char btn[16];
        int wait;
        /* "<frames to wait> <BUTTON>" */
        if (sscanf(line, "%d %15s", &wait, btn) == 2) {
            frame += wait;
            demo[ndemo].frame = frame;
            demo[ndemo].buttons = button_by_name(btn);
            ndemo++;
        }
    }
    free(txt);
}

static void demo_step(void) {
    if (!demo) return;
    demo_frame++;
    while (demo_i < ndemo && demo[demo_i].frame <= demo_frame) input_inject(demo[demo_i++].buttons);
}

bool app_demo_active(void) { return demo && demo_i < ndemo; }

/* ---------------- library state ---------------- */

static game_t *cur_game(void) { return nview ? &g_games[view[cur]] : NULL; }

static void select_changed(bool sound) {
    sel_t = 0;
    media_started = false;
    clip_play(NULL);
    details_scroll = 0;
    if (nview) snprintf(g_set.last_game, sizeof g_set.last_game, "%s", g_games[view[cur]].id);
    if (sound) audio_sfx(SFX_MOVE);
}

static void rebuild_view(const char *keep_id) {
    nview = library_view(view, filter, g_set.sort);
    cur = 0;
    for (int i = 0; i < nview; i++) if (keep_id && !strcmp(g_games[view[i]].id, keep_id)) { cur = i; break; }
    pos_s = cur;
    select_changed(false);
}

static void start_media(void) {
    game_t *g = cur_game();
    media_started = true;
    if (!g) return;
    char rel[96], path[160];
    if (g->has_clip && g_set.bg_video) {
        game_asset_path(rel, sizeof rel, g, "clip.cmv");
        clip_play(fs_path(path, sizeof path, rel));
    } else clip_play(NULL);
    if (g->has_music && g_set.game_music) {
        game_asset_path(rel, sizeof rel, g, "music.mp3");
        audio_music(fs_path(path, sizeof path, rel));
    } else audio_music(NULL);
}

static void move(int d) {
    if (!nview) return;
    int n = cur + d;
    if (n < 0) n = 0;
    if (n >= nview) n = nview - 1;
    if (n != cur) { cur = n; select_changed(true); }
}

static void jump_letter(int dir) {
    if (!nview || g_set.sort != SORT_NAME || filter == FILTER_RECENT) { move(dir * 10); return; }
    char c0 = toupper((unsigned char)g_games[view[cur]].name[0]);
    int i = cur;
    while (i + dir >= 0 && i + dir < nview) {
        i += dir;
        char c = toupper((unsigned char)g_games[view[i]].name[0]);
        if (c != c0) {
            if (dir < 0) while (i > 0 && toupper((unsigned char)g_games[view[i - 1]].name[0]) == c) i--;
            break;
        }
    }
    move(i - cur);
}

static const char *filter_name(int f) { return tr(f == FILTER_FAV ? S_FAVORITES : f == FILTER_RECENT ? S_RECENTLY : S_ALL_GAMES); }

static void meta_line(char *buf, int n, const game_t *g) {
    char pt[24];
    buf[0] = 0;
    int k = 0;
    if (g->year) k += snprintf(buf + k, n - k, "%d", g->year);
    if (g->publisher[0]) k += snprintf(buf + k, n - k, "%s%s", k ? "  ·  " : "", g->publisher);
    if (g->genre[0]) k += snprintf(buf + k, n - k, "%s%s", k ? "  ·  " : "", game_genre(g, g_set.lang));
    if (g->players[0] && strcmp(g->players, "0")) k += snprintf(buf + k, n - k, "%s%sP", k ? "  ·  " : "", g->players);
    (void)pt;
}

/* "há 3 dias": the console's clock may be years off, but two readings of it still subtract fine */
static void fmt_ago(char *buf, int n, uint32_t when) {
    long d = (long)time(NULL) - (long)when;
    if (d < 3600) snprintf(buf, n, "%s", tr(S_AGO_NOW));
    else if (d < 86400) snprintf(buf, n, tr(S_AGO_HOURS), (int)(d / 3600));
    else if (d < 2 * 86400) snprintf(buf, n, "%s", tr(S_AGO_YESTERDAY));
    else if (d < 14 * 86400) snprintf(buf, n, tr(S_AGO_DAYS), (int)(d / 86400));
    else if (d < 60 * 86400) snprintf(buf, n, tr(S_AGO_WEEKS), (int)(d / (7 * 86400)));
    else snprintf(buf, n, tr(S_AGO_MONTHS), (int)(d / (30 * 86400)));
}

/* a small clock: the play-time mark */
static void clock_icon(float cx, float cy, float r, uint32_t c) {
    draw_ring(cx, cy, r - 1.6f, r, c);
    float hand[8] = {cx - 0.8f, cy - r * 0.62f, cx + 0.8f, cy - r * 0.62f, cx + 0.8f, cy + 0.6f, cx - 0.8f, cy + 0.6f};
    float hand2[8] = {cx - 0.6f, cy - 0.8f, cx + r * 0.5f, cy - 0.8f, cx + r * 0.5f, cy + 0.8f, cx - 0.6f, cy + 0.8f};
    uint32_t cs[4] = {c, c, c, c};
    draw_quad_flat(hand, cs);
    draw_quad_flat(hand2, cs);
}

/* "Tempo de jogo 2h 30m  ·  Última vez há 3 dias", or "Nunca jogado"; x is the left edge, or the
 * centre when centred */
static void playtime_line(const game_t *g, float x, float y, bool centred, float maxw, float a) {
    char buf[96], pt[24], ago[32];
    if (g->playtime || g->last_played) {
        fmt_playtime(pt, sizeof pt, g->playtime);
        fmt_ago(ago, sizeof ago, g->last_played);
        snprintf(buf, sizeof buf, "%s %s  ·  %s %s", tr(S_PLAYTIME), pt, tr(S_LAST_PLAYED), ago);
        if (text_width(&g_font_s, buf) > maxw - 22) snprintf(buf, sizeof buf, "%s  ·  %s", pt, ago);
    } else snprintf(buf, sizeof buf, "%s", tr(S_NEVER_PLAYED));
    float w = text_width(&g_font_s, buf) + 22;
    float x0 = centred ? x - w / 2 : x;
    uint32_t c = col_alpha(g->playtime ? th->accent : th->text_dim, a);
    clock_icon(x0 + 8, y + g_font_s.ascent * 0.55f + 1, 7.5f, c);
    draw_text_fit(&g_font_s, x0 + 22, y, maxw - 22, c, buf);
}

/* ---------------- launch ---------------- */

static void before_jump(void) { fs_shutdown(); }

/* Swiss on its own: an empty Autoload cancels the "load at startup" that brings Swiss back to the menu */
static const char *NO_AUTOLOAD[1] = {"Autoload="};

static void do_boot(void) {
    game_t *g = &g_games[launch_game];
    char err[96], launch_path[256], arg[300];
    const char *swiss = boot_find_swiss();
    stats_mark_launch(g);
    snprintf(g_set.last_game, sizeof g_set.last_game, "%s", g->id);
    settings_save();
    game_launch_path(launch_path, sizeof launch_path, g, launch_disc);
    snprintf(arg, sizeof arg, "Autoload=%s", launch_path);
    const char *args[1] = {arg};
    if (!swiss) { ui_toast(tr(S_SWISS_MISSING)); screen = SCR_LIBRARY; return; }
    clip_stop();
    assets_stop();
    audio_stop();
    boot_dol(swiss, 1, args, before_jump, err, sizeof err);
    /* only here when the DOL could not be loaded; the menu keeps running */
    char msg[160];
    snprintf(msg, sizeof msg, "%s: %s", tr(S_BOOT_ERROR), err);
    clip_init();
    assets_init();
    audio_init();
    ui_toast(msg);
    screen = SCR_LIBRARY;
}

static void open_swiss(void) {
    const char *swiss = boot_find_swiss();
    if (!swiss) { ui_toast(tr(S_SWISS_MISSING)); return; }
    char err[96];
    settings_save();
    clip_stop(); assets_stop(); audio_stop();
    boot_dol(swiss, 1, NO_AUTOLOAD, before_jump, err, sizeof err);
    clip_init(); assets_init(); audio_init();
    ui_toast(err);
}

static void start_launch(int gi, int disc) {
    if (!boot_find_swiss()) { ui_toast(tr(S_SWISS_MISSING)); audio_sfx(SFX_BACK); return; }
    launch_game = gi;
    launch_disc = disc;
    launch_t = 0;
    sidebar = false;
    disc_game = -1;
    if (!g_set.launch_anim) { do_boot(); return; }
    screen = SCR_LAUNCH;
    clip_play(NULL);
    audio_music("");
    audio_sfx(SFX_LAUNCH);
}

static void request_launch(int gi) {
    if (g_games[gi].ndisc > 1) { disc_game = gi; disc_cur = 0; audio_sfx(SFX_OPEN); return; }
    audio_sfx(SFX_SELECT);
    start_launch(gi, 0);
}

/* ---------------- intro ---------------- */

static void draw_intro(void) {
    float t = screen_t;
    static bool chimed;
    if (!chimed && t > 1.15f) { chimed = true; audio_sfx(SFX_INTRO); }
    draw_rect_grad(0, 0, SCREEN_W, SCREEN_H, 0x000000FF, col_mix(0x000000FF, th->bg_top, clampf(t / 1.5f, 0, 1)));
    /* a cube tumbles in from the left and lands in the middle (a nod to the console's boot) */
    float k = clampf(t / 1.25f, 0, 1), e = ease_out(k);
    float x = lerpf(-260, 0, e), y = 40 + 120 * fabsf(sinf(k * M_PI * 2.0f)) * (1 - k);
    float roll = (1 - e) * 360;
    float fade_cube = 1 - clampf((t - 1.35f) / 0.35f, 0, 1);
    gfx_3d(36, (vec3){0, 140, 520}, (vec3){0, 30, 0});
    if (fade_cube > 0) {
        gfx_push();
        gfx_translate(x, y, 0);
        gfx_rotate('z', roll);
        gfx_rotate('y', 45);
        gfx_rotate('x', 0);
        box3(-34, -34, -34, 34, 34, 34, th->console, fade_cube);
        gfx_pop();
    }
    gfx_2d();
    float la = clampf((t - 1.2f) / 0.5f, 0, 1);
    float s = 0.86f + 0.14f * ease_out(la);
    float w = 320 * s, h = 288 * s;
    draw_glow(SCREEN_W / 2, 200, 220, col_alpha(th->accent2, 0.35f * la));
    draw_tex(&g_tex_logo, (SCREEN_W - w) / 2, 200 - h / 2 + 20, w, h, col_alpha(0xFFFFFFFF, la));
    float fa = clampf((t - 1.8f) / 0.5f, 0, 1);
    if (fa > 0) draw_text_center(&g_font_s, SCREEN_W / 2, 400, col_alpha(th->text_dim, fa), "v" CUBEMENU_VERSION);
    float out = clampf((t - 2.8f) / 0.4f, 0, 1);
    if (out > 0) draw_rect(0, 0, SCREEN_W, SCREEN_H, col_alpha(th->bg_top, out));
}

/* ---------------- library: coverflow ---------------- */

#define FLOOR_Y (-CASE_H / 2 - 1)

static void place(int i, float *x, float *z, float *ry, float *alpha) {
    float d = i - pos_s, ad = fabsf(d), s = d < 0 ? -1 : 1;
    if (ad < 1) { *x = d * 178; *z = 60 - ad * 120; *ry = -s * ad * 64; }
    else { *x = s * (178 + (ad - 1) * 50); *z = -60 - (ad - 1) * 4; *ry = -s * 64; }
    *alpha = clampf(1 - (ad - 5.0f) / 1.5f, 0, 1);
}

static int cmp_dist(const void *a, const void *b) {
    float da = fabsf(*(const int *)a - pos_s), db = fabsf(*(const int *)b - pos_s);
    return da < db ? 1 : da > db ? -1 : 0;
}

static void draw_case_at(int i, bool reflect) {
    float x, z, ry, al;
    place(i, &x, &z, &ry, &al);
    if (al <= 0) return;
    int g = view[i];
    bool selected = i == cur && fabsf(i - pos_s) < 0.5f;
    float disc_out = 0, sway = 0;
    if (selected) {
        if (g_set.show_disc) disc_out = ease_io((sel_t - 0.35f) / 0.7f);
        sway = sinf(ui_time() * 0.9f) * 6 * clampf(sel_t / 0.8f, 0, 1);
    }
    gfx_push();
    if (reflect) { gfx_translate(0, 2 * FLOOR_Y, 0); gfx_scale(1, -1, 1); }
    gfx_translate(x, 0, z);
    gfx_rotate('y', ry + sway);
    case3d(assets_get(g, AS_CASE), assets_get(g, AS_DISC), al, disc_out, ui_time() * 2.2f, reflect);
    gfx_pop();
}

static void draw_coverflow(void) {
    gfx_3d(34, (vec3){0, 30, 760}, (vec3){0, -40, 0});
    int idx[20], n = 0;
    for (int i = (int)floorf(pos_s) - 7; i <= (int)ceilf(pos_s) + 7 && n < 20; i++)
        if (i >= 0 && i < nview) idx[n++] = i;
    qsort(idx, n, sizeof *idx, cmp_dist);
    /* floor: a soft band the reflections sit in */
    gfx_depth(false);
    uint32_t f0 = col_alpha(th->floor, 0), f1 = col_alpha(th->floor, 0.55f);
    quad3(NULL, (vec3){-900, FLOOR_Y, -400}, (vec3){900, FLOOR_Y, -400}, (vec3){900, FLOOR_Y, 300}, (vec3){-900, FLOOR_Y, 300}, 0, 0, 0, 0, f0, f0, f1, f1);
    for (int k = 0; k < n; k++) draw_case_at(idx[k], true);
    gfx_depth(true);
    for (int k = 0; k < n; k++) draw_case_at(idx[k], false);
    gfx_2d();
}

/* ---------------- library: grid ---------------- */

#define GRID_COLS 6
#define GRID_CW 92
#define GRID_RH 122
static float grid_scroll;

static void draw_cover2d(int g, float x, float y, float w, float h, float a) {
    const tex_t *t = assets_get(g, AS_CASE);
    draw_round_rect(x - 2, y - 2, w + 4, h + 4, 4, col_alpha(0x000000FF, 0.45f * a));
    if (t) draw_tex_uv(t, x, y, w, h, 0.5254f, 0, 1, 1, col_alpha(0xFFFFFFFF, a));
    else {
        draw_rect_grad(x, y, w, h, col_alpha(0x2B2B33FF, a), col_alpha(0x111116FF, a));
        draw_tex(&g_tex_emblem, x + w * 0.25f, y + h / 2 - w * 0.25f, w * 0.5f, w * 0.5f, col_alpha(0xFFFFFFFF, 0.5f * a));
        draw_text_fit(&g_font_s, x + 6, y + h - 26, w - 12, col_alpha(0xFFFFFFFF, a), g_games[g].name);
    }
}

static void draw_grid(void) {
    const float cw = GRID_CW, gx0 = (SCREEN_W - GRID_COLS * cw) / 2 + 8, gy0 = 90;
    int row = cur / GRID_COLS, rows = (nview + GRID_COLS - 1) / GRID_COLS;
    /* two rows on screen; the cursor row is the lower one unless it is the first */
    float target = (row > 0 ? row - 1 : 0) * GRID_RH;
    if (rows > 1 && target > (rows - 2) * GRID_RH) target = (rows - 2) * GRID_RH;
    grid_scroll += (target - grid_scroll) * 0.18f;
    for (int i = 0; i < nview; i++) {
        int r = i / GRID_COLS, c = i % GRID_COLS;
        float y = gy0 + r * GRID_RH - grid_scroll;
        if (y < gy0 - 70 || y > gy0 + GRID_RH + 70) continue;
        float a = clampf((y - (gy0 - 70)) / 70, 0, 1) * clampf((gy0 + GRID_RH + 70 - y) / 70, 0, 1);
        float x = gx0 + c * cw, w = 74, h = 104;
        if (i == cur) {
            float s = 1.08f;
            float sw = w * s, sh = h * s;
            draw_glow(x + w / 2, y + h / 2, 92, col_alpha(th->accent, 0.35f * a));
            draw_round_rect(x + (w - sw) / 2 - 4, y + (h - sh) / 2 - 4, sw + 8, sh + 8, 6, col_alpha(th->accent, a));
            draw_cover2d(view[i], x + (w - sw) / 2, y + (h - sh) / 2, sw, sh, a);
        } else draw_cover2d(view[i], x, y, w, h, a * 0.85f);
        if (g_games[view[i]].favorite) draw_circle(x + w - 8, y + 8, 5, col_alpha(0xF4C21AFF, a));
    }
}

/* ---------------- library: list ---------------- */

static float list_scroll;

static void draw_list(void) {
    const float x = 40, y0 = 88, rh = 27, vis = 10;
    float target = cur - vis / 2 + 0.5f;
    if (target > nview - vis) target = nview - vis;
    if (target < 0) target = 0;
    list_scroll += (target - list_scroll) * 0.2f;
    ui_panel(x - 14, y0 - 10, 300, vis * rh + 20, 1);
    for (int i = (int)list_scroll; i < nview && i < list_scroll + vis + 1; i++) {
        float y = y0 + (i - list_scroll) * rh;
        if (y < y0 - 4 || y > y0 + (vis - 1) * rh + 4) continue;
        game_t *g = &g_games[view[i]];
        if (i == cur) draw_round_rect(x - 8, y - 2, 288, rh - 2, 8, col_alpha(th->accent, 0.9f));
        draw_text_fit(&g_font_s, x, y + 2, 240, i == cur ? 0x14102EFF : th->text, g->name);
        if (g->favorite) draw_circle(x + 262, y + 11, 4, 0xF4C21AFF);
    }
    ui_scrollbar(x + 290, y0, vis * rh - 6, nview > vis ? list_scroll / (nview - vis) : 0, vis / (float)(nview ? nview : 1));
    /* the selected case, turning slowly, on the right */
    game_t *g = cur_game();
    if (!g) return;
    gfx_3d(34, (vec3){0, 26, 690}, (vec3){0, -34, 0});
    gfx_push();
    gfx_translate(150, 6, 40);
    gfx_rotate('y', -18 + sinf(ui_time() * 0.6f) * 14);
    float disc_out = g_set.show_disc ? ease_io((sel_t - 0.35f) / 0.7f) : 0;
    case3d(assets_get(view[cur], AS_CASE), assets_get(view[cur], AS_DISC), 1, disc_out, ui_time() * 2.2f, false);
    gfx_pop();
    gfx_2d();
}

/* ---------------- library screen ---------------- */

static void draw_title_block(void) {
    game_t *g = cur_game();
    if (!g) return;
    float y = g_set.view == VIEW_LIST ? 356 : 348;
    float a = clampf(sel_t / 0.15f, 0.4f, 1);
    if (g_set.view == VIEW_LIST) {
        draw_text_fit(&g_font_m, 360, y, 250, col_alpha(th->text_hi, a), g->name);
        char meta[160];
        meta_line(meta, sizeof meta, g);
        draw_text_fit(&g_font_s, 360, y + 30, 250, col_alpha(th->text_dim, a), meta);
        playtime_line(g, 360, y + 52, false, 250, a);
        return;
    }
    char name[80];
    snprintf(name, sizeof name, "%s", g->name);
    float w = text_width(&g_font_l, name);
    const font_t *f = w > 560 ? &g_font_m : &g_font_l;
    draw_text_center(f, SCREEN_W / 2 + 1, y + 2, col_alpha(0x00000080, a), name);
    draw_text_center(f, SCREEN_W / 2, y, col_alpha(th->text_hi, a), name);
    if (g->favorite) {
        float sx = SCREEN_W / 2 + text_width(f, name) / 2 + 14;
        draw_circle(sx, y + f->ascent * 0.55f, 6, 0xF4C21AFF);
    }
    char meta[160];
    meta_line(meta, sizeof meta, g);
    draw_text_center(&g_font_s, SCREEN_W / 2, y + 36, col_alpha(th->text_dim, a), meta);
    playtime_line(g, SCREEN_W / 2, y + 58, true, 560, a);
}

static void draw_sidebar(void) {
    if (sidebar_a <= 0.01f) return;
    float w = 300, x = SCREEN_W - w * ease_out(sidebar_a);
    draw_rect(0, 0, SCREEN_W, SCREEN_H, col_alpha(0x00000080, sidebar_a));
    draw_rect_grad(x, 0, w, SCREEN_H, col_alpha(th->bg_top, 0.97f), col_alpha(th->bg_bottom, 0.97f));
    draw_rect(x, 0, 2, SCREEN_H, col_alpha(th->accent, 0.6f));
    draw_text(&g_font_m, x + 24, 30, th->text_hi, tr(S_RECENTLY));
    if (!nside) draw_text(&g_font_s, x + 24, 80, th->text_dim, tr(S_NEVER_PLAYED));
    for (int i = 0; i < nside; i++) {
        game_t *g = &g_games[side_list[i]];
        float y = 76 + i * 40;
        if (i == side_cur) draw_round_rect(x + 14, y - 4, w - 28, 38, 8, col_alpha(th->accent, 0.9f));
        const tex_t *c = assets_get(side_list[i], AS_CASE);
        if (c) draw_tex_uv(c, x + 22, y, 22, 30, 0.5254f, 0, 1, 1, 0xFFFFFFFF);
        draw_text_fit(&g_font_s, x + 52, y, w - 80, i == side_cur ? 0x14102EFF : th->text_hi, g->name);
        char pt[24];
        fmt_playtime(pt, sizeof pt, g->playtime);
        draw_text(&g_font_s, x + 52, y + 16, i == side_cur ? 0x2A2350FF : th->text_dim, pt);
    }
}

static void draw_disc_modal(void) {
    if (disc_game < 0) return;
    game_t *g = &g_games[disc_game];
    draw_rect(0, 0, SCREEN_W, SCREEN_H, 0x000000A0);
    float w = 120 + g->ndisc * 140, h = 230, x = (SCREEN_W - w) / 2, y = 120;
    ui_panel(x, y, w, h, 1.4f);
    draw_text_center(&g_font_m, SCREEN_W / 2, y + 18, th->text_hi, tr(S_CHOOSE_DISC));
    const tex_t *dt = assets_get(disc_game, AS_DISC);
    for (int i = 0; i < g->ndisc; i++) {
        float cx = x + 130 + i * 140, cy = y + 112;
        bool sel = i == disc_cur;
        if (sel) draw_glow(cx, cy, 74, col_alpha(th->accent, 0.5f));
        if (dt) draw_tex_disc(dt, cx, cy, sel ? 52 : 44, sel ? ui_time() * 2 : 0, sel ? 0xFFFFFFFF : 0xFFFFFFA0);
        else draw_circle(cx, cy, sel ? 52 : 44, 0xD8D8E0FF);
        draw_circle(cx, cy, 8, 0x14141AFF);
        char s[24];
        snprintf(s, sizeof s, "%s %d", tr(S_DISC), i + 1);
        draw_text_center(&g_font_m, cx, cy + 62, sel ? th->text_hi : th->text_dim, s);
    }
}

static void update_library(uint32_t p, float dt) {
    if (disc_game >= 0) {
        if (p & (BTN_LEFT | BTN_UP)) { if (disc_cur > 0) { disc_cur--; audio_sfx(SFX_MOVE); } }
        if (p & (BTN_RIGHT | BTN_DOWN)) { if (disc_cur < g_games[disc_game].ndisc - 1) { disc_cur++; audio_sfx(SFX_MOVE); } }
        if (p & BTN_A) { audio_sfx(SFX_SELECT); start_launch(disc_game, disc_cur); }
        if (p & BTN_B) { disc_game = -1; audio_sfx(SFX_BACK); }
        return;
    }
    if (sidebar) {
        if (p & BTN_UP && side_cur > 0) { side_cur--; audio_sfx(SFX_MOVE); }
        if (p & BTN_DOWN && side_cur < nside - 1) { side_cur++; audio_sfx(SFX_MOVE); }
        if (p & BTN_A && nside) request_launch(side_list[side_cur]);
        if (p & (BTN_B | BTN_LEFT)) { sidebar = false; audio_sfx(SFX_BACK); }
        return;
    }
    if (!nview) {
        if (p & BTN_X) { screen = SCR_SETTINGS; screen_t = 0; set_cur = I_THEME; audio_sfx(SFX_OPEN); }
        if (p & BTN_START) { screen = SCR_SYSTEM; screen_t = 0; audio_sfx(SFX_OPEN); }
        if (p & (BTN_L | BTN_R)) { filter = (filter + (p & BTN_R ? 1 : FILTER_COUNT - 1)) % FILTER_COUNT; rebuild_view(NULL); audio_sfx(SFX_OPEN); }
        return;
    }
    int step_h = 1, step_v = 0;
    if (g_set.view == VIEW_GRID) step_v = GRID_COLS;
    if (g_set.view == VIEW_LIST) { step_v = 1; step_h = 10; }
    if (p & BTN_LEFT) move(-step_h);
    if (p & BTN_RIGHT) move(step_h);
    if (step_v && (p & BTN_UP)) move(-step_v);
    if (step_v && (p & BTN_DOWN)) move(step_v);
    if (p & BTN_CLEFT) jump_letter(-1);
    if (p & BTN_CRIGHT) jump_letter(1);
    if (p & (BTN_L | BTN_R)) {
        filter = (filter + (p & BTN_R ? 1 : FILTER_COUNT - 1)) % FILTER_COUNT;
        game_t *g = cur_game();
        rebuild_view(g ? g->id : NULL);
        audio_sfx(SFX_OPEN);
        char msg[64];
        snprintf(msg, sizeof msg, "%s  (%d)", filter_name(filter), nview);
        ui_toast(msg);
    }
    if (p & BTN_A) request_launch(view[cur]);
    if (p & BTN_Y) { screen = SCR_DETAILS; screen_t = 0; details_scroll = 0; details_shot = 0; audio_sfx(SFX_OPEN); }
    if (p & BTN_X) { screen = SCR_SETTINGS; screen_t = 0; set_cur = I_THEME; audio_sfx(SFX_OPEN); }
    if (p & BTN_START) { screen = SCR_SYSTEM; screen_t = 0; audio_sfx(SFX_OPEN); }
    if (p & BTN_Z) {
        game_t *g = cur_game();
        g->favorite = !g->favorite;
        stats_save();
        ui_toast(tr(g->favorite ? S_FAV_ADDED : S_FAV_REMOVED));
        audio_sfx(SFX_SELECT);
        if (filter == FILTER_FAV) rebuild_view(g->id);
    }
    if (p & BTN_B) {
        int all[MAX_GAMES];
        int n = library_view(all, FILTER_RECENT, SORT_RECENT);
        nside = n > 10 ? 10 : n;
        memcpy(side_list, all, nside * sizeof *side_list);
        side_cur = 0;
        sidebar = true;
        audio_sfx(SFX_OPEN);
    }
    (void)dt;
}

static void draw_library(void) {
    ui_background(g_set.bg_video ? 0.72f : 0);
    char sub[48];
    snprintf(sub, sizeof sub, tr(S_N_GAMES), nview);
    ui_topbar(filter_name(filter), sub);
    if (!card_ok || !nview) {
        draw_tex(&g_tex_logo, (SCREEN_W - 200) / 2, 110, 200, 180, col_alpha(0xFFFFFFFF, 0.5f));
        int msg = !card_ok ? S_NO_CARD : filter == FILTER_FAV && g_ngames ? S_NO_FAVORITES : filter == FILTER_RECENT && g_ngames ? S_NO_RECENT : S_NO_GAMES;
        draw_text_center(&g_font_m, SCREEN_W / 2, 300, th->text_hi, tr(msg));
        if (msg == S_NO_GAMES || msg == S_NO_CARD) draw_text_center(&g_font_s, SCREEN_W / 2, 334, th->text_dim, tr(S_NO_GAMES_HINT));
        else draw_text_center(&g_font_s, SCREEN_W / 2, 334, th->text_dim, tr(msg == S_NO_FAVORITES ? S_NO_FAVORITES_HINT : S_NO_RECENT_HINT));
        static const int gl[] = {G_LR, G_X, G_START};
        const char *lb[] = {tr(S_CATEGORY), tr(S_SETTINGS), tr(S_SYSTEM)};
        ui_hints(gl, lb, 3);
        return;
    }
    if (g_set.view == VIEW_GRID) draw_grid();
    else if (g_set.view == VIEW_LIST) draw_list();
    else draw_coverflow();
    draw_title_block();
    char idx[24];
    snprintf(idx, sizeof idx, "%d / %d", cur + 1, nview);
    draw_text_right(&g_font_s, SCREEN_W - 36, 60, th->text_dim, idx);
    static const int gl[] = {G_A, G_Y, G_X, G_B, G_LR};
    const char *lb[] = {tr(S_PLAY), tr(S_DETAILS), tr(S_SETTINGS), tr(S_RECENT), tr(S_CATEGORY)};
    ui_hints(gl, lb, 5);
    draw_sidebar();
    draw_disc_modal();
}

/* ---------------- details ---------------- */

static void update_details(uint32_t p) {
    game_t *g = cur_game();
    if (!g) { screen = SCR_LIBRARY; return; }
    if (disc_game >= 0) { update_library(p, 0); return; }
    if (p & (BTN_L | BTN_LEFT)) { move(-1); details_scroll = 0; }
    if (p & (BTN_R | BTN_RIGHT)) { move(1); details_scroll = 0; }
    if (p & BTN_UP) details_scroll -= 1;
    if (p & BTN_DOWN) details_scroll += 1;
    if (details_scroll > details_lines - 8) details_scroll = details_lines - 8;
    if (details_scroll < 0) details_scroll = 0;
    if (p & BTN_X) details_shot ^= 1;
    if (p & BTN_A) request_launch(view[cur]);
    if (p & BTN_Y) { g->favorite = !g->favorite; stats_save(); audio_sfx(SFX_SELECT); }
    if (p & BTN_Z) { audio_sfx(SFX_OPEN); mc_open_game(view[cur]); return; }
    if (p & BTN_B) { screen = SCR_LIBRARY; audio_sfx(SFX_BACK); }
    assets_want_shots(view[cur]);
}

static float kv_w = 140;
static void kv(float x, float y, const char *k, const char *v) {
    draw_text_fit(&g_font_s, x, y, kv_w - 8, th->text_dim, k);
    draw_text_fit(&g_font_s, x + kv_w, y, 270 - kv_w, th->text_hi, v && *v ? v : "-");
}

static void draw_details(void) {
    game_t *g = cur_game();
    if (!g) return;
    int gi = view[cur];
    ui_background(g_set.bg_video ? 0.5f : 0);
    ui_topbar(tr(S_DETAILS), NULL);
    float in = ease_out(screen_t / 0.35f);
    float lx = 36 - (1 - in) * 30, rx = 330 + (1 - in) * 30;

    /* mini video (falls back to a screenshot, then the cover) */
    float vw = 272, vh = 153, vy = 82;
    draw_round_rect(lx - 4, vy - 4, vw + 8, vh + 8, 8, col_alpha(0x000000FF, 0.6f * in));
    const tex_t *f = clip_frame();
    const tex_t *snap = assets_get(gi, AS_SNAP), *title = assets_get(gi, AS_TITLE), *cs = assets_get(gi, AS_CASE);
    if (f && g->has_clip) draw_tex(f, lx, vy, vw, vh, col_alpha(0xFFFFFFFF, in * clampf(clip_age() / 0.4f, 0, 1)));
    else if (snap) draw_tex(snap, lx, vy, vw, vh, col_alpha(0xFFFFFFFF, in));
    else if (cs) draw_tex_uv(cs, lx + vw / 2 - 54, vy, 108, vh, 0.5254f, 0, 1, 1, col_alpha(0xFFFFFFFF, in));
    else draw_rect_grad(lx, vy, vw, vh, 0x2B2B33FF, 0x111116FF);
    if (f && g->has_clip) {
        draw_round_rect(lx + 8, vy + 8, 54, 18, 9, 0x000000A0);
        draw_circle(lx + 18, vy + 17, 4, 0xE2343AFF);
        draw_text(&g_font_s, lx + 26, vy + 7, 0xFFFFFFFF, tr(S_VIDEO));
    }
    /* screenshots */
    float sy = vy + vh + 16;
    draw_text(&g_font_s, lx, sy, th->text_dim, tr(S_SCREENSHOTS));
    const tex_t *shots[2] = {snap, title};
    for (int i = 0; i < 2; i++) {
        float x = lx + i * 140, y = sy + 22, w = 132, h = 74;   /* 4:3 shots shown cropped to fit the stats below */
        bool sel = details_shot == i;
        draw_round_rect(x - 3, y - 3, w + 6, h + 6, 6, sel ? col_alpha(th->accent, in) : col_alpha(0x000000FF, 0.5f * in));
        if (shots[i]) draw_tex_uv(shots[i], x, y, w, h, 0, 0.11f, 1, 0.89f, col_alpha(0xFFFFFFFF, in));
        else {
            draw_rect_grad(x, y, w, h, 0x2B2B33FF, 0x111116FF);
            draw_tex(&g_tex_emblem, x + w / 2 - 22, y + h / 2 - 22, 44, 44, 0xFFFFFF60);
        }
    }
    /* play stats: time, last played, sessions; one row each, label left, value right */
    {
        float py = sy + 22 + 74 + 9, pw = 272, rh = 19;
        ui_panel(lx - 4, py, pw + 8, rh * 3 + 9, 1.2f * in);
        char v[3][32];
        if (g->playtime || g->last_played) { fmt_playtime(v[0], 32, g->playtime); fmt_ago(v[1], 32, g->last_played); }
        else { snprintf(v[0], 32, "%s", tr(S_NEVER_PLAYED)); snprintf(v[1], 32, "-"); }
        snprintf(v[2], 32, "%u", (unsigned)g->launches);
        const int lab[3] = {S_PLAYTIME, S_LAST_PLAYED, S_LAUNCHES};
        for (int i = 0; i < 3; i++) {
            float ry = py + 4 + i * rh;
            uint32_t vc = i == 0 && g->playtime ? th->accent : th->text_hi;
            if (i == 0) clock_icon(lx + 8, ry + g_font_s.ascent * 0.55f + 1, 7, col_alpha(vc, in));
            draw_text(&g_font_s, lx + 22, ry, col_alpha(th->text_dim, in), tr(lab[i]));
            draw_text_right(&g_font_s, lx + pw - 6, ry, col_alpha(vc, in), v[i]);
        }
    }
    /* the mini disc, spinning next to the shots */
    const tex_t *dt = assets_get(gi, AS_DISC);
    float dcx = lx + vw - 8, dcy = vy + vh - 6;
    if (dt) { draw_circle(dcx + 2, dcy + 3, 37, 0x00000070); draw_tex_disc(dt, dcx, dcy, 36, ui_time() * 1.6f, col_alpha(0xFFFFFFFF, in)); draw_circle(dcx, dcy, 6, 0x101014FF); }

    /* right column: title, facts, synopsis */
    float y = 80;
    int lines = draw_text_wrap(&g_font_l, rx, y, 274, 34, 0, 2, th->text_hi, g->name);
    y += (lines > 1 ? 2 : 1) * 34 + 8;
    char buf[64], pt[24];
    kv(rx, y, tr(S_DEVELOPER), g->developer); y += 21;
    kv(rx, y, tr(S_PUBLISHER), g->publisher); y += 21;
    snprintf(buf, sizeof buf, "%d", g->year);
    kv(rx, y, tr(S_YEAR), g->year ? buf : NULL); y += 21;
    kv(rx, y, tr(S_GENRE), game_genre(g, g_set.lang)); y += 21;
    kv(rx, y, tr(S_PLAYERS), g->players); y += 21;
    (void)pt;
    if (g->ndisc > 1) { snprintf(buf, sizeof buf, tr(S_DISCS), g->ndisc); kv(rx, y, tr(S_DISC), buf); y += 21; }
    y += 8;
    float sh = 418 - y;
    ui_panel(rx - 12, y - 8, 290, sh + 10, 1);
    const char *syn = game_synopsis(g, g_set.lang);
    int vis = (int)(sh / 19);
    details_lines = draw_text_wrap(&g_font_s, rx, y, 262, 19, (int)details_scroll, vis, th->text, syn ? syn : tr(S_NO_SYNOPSIS));
    if (details_lines > vis) ui_scrollbar(rx + 268, y, sh - 6, details_scroll / (float)(details_lines - vis), vis / (float)details_lines);
    if (g->favorite) draw_circle(rx + 268, 92, 6, 0xF4C21AFF);

    static const int gl[] = {G_A, G_B, G_Y, G_Z, G_LR, G_DPAD};
    const char *lb[] = {tr(S_PLAY), tr(S_BACK), tr(S_FAVORITE), tr(S_SAVES), tr(S_PREV_NEXT), tr(S_SCROLL)};
    ui_hints(gl, lb, 6);
    draw_disc_modal();
}

/* ---------------- settings ---------------- */



static bool is_header(int i) { return i == I_H_VISUAL || i == I_H_AUDIO || i == I_H_LIBRARY || i == I_H_SYSTEM; }

static int item_label(int i) {
    static const int L[I_COUNT] = {S_SECTION_VISUAL, S_THEME, S_LANGUAGE, S_VIEW, S_BG_VIDEO, S_SHOW_DISC, S_LAUNCH_ANIM,
        S_SECTION_AUDIO, S_GAME_MUSIC, S_MENU_MUSIC, S_UI_SOUNDS, S_VOLUME, S_SECTION_LIBRARY, S_SORT,
        S_SECTION_SYSTEM, S_CLOCK_24H, S_CLOCK_ADJUST, S_SCREENSAVER, S_OPEN_SWISS, S_RESET_DEFAULTS};
    return L[i];
}

static const char *onoff(bool b) { return tr(b ? S_ON : S_OFF); }

static const char *item_value(int i, char *buf, int n) {
    switch (i) {
    case I_THEME: return g_themes[g_set.theme].name;
    case I_LANG: return LANG_NAMES[g_set.lang];
    case I_VIEW: return tr(S_VIEW_COVERFLOW + g_set.view);
    case I_BGVIDEO: return onoff(g_set.bg_video);
    case I_SHOWDISC: return onoff(g_set.show_disc);
    case I_LAUNCHANIM: return onoff(g_set.launch_anim);
    case I_GAMEMUSIC: return onoff(g_set.game_music);
    case I_MENUMUSIC: return onoff(g_set.menu_music);
    case I_SFX: return onoff(g_set.sfx);
    case I_VOLUME: snprintf(buf, n, "%d%%", g_set.volume * 10); return buf;
    case I_SORT: return tr(S_SORT_NAME + g_set.sort);
    case I_CLOCK24: return onoff(g_set.clock24);
    case I_CLOCKADJ: { char clk[16]; ui_clock_string(clk, sizeof clk);
        snprintf(buf, n, "%s  (%+d:%02d)", clk, g_set.clock_offset / 60, abs(g_set.clock_offset) % 60); return buf; }
    case I_SAVER: if (!g_set.screensaver) return tr(S_OFF); snprintf(buf, n, tr(S_MINUTES), g_set.screensaver); return buf;
    case I_OPENSWISS: { const char *s = boot_find_swiss(); return s ? strchr(s, ':') + 1 : tr(S_NOT_FOUND); }
    }
    return "";
}

static void change_item(int i, int d) {
    static const int savers[] = {0, 1, 3, 5, 10, 15, 30};
    switch (i) {
    case I_THEME: g_set.theme = (g_set.theme + d + THEME_COUNT) % THEME_COUNT; theme_set(g_set.theme); break;
    case I_LANG: g_set.lang = (g_set.lang + d + LANG_COUNT) % LANG_COUNT; lang_set(g_set.lang); break;
    case I_VIEW: g_set.view = (g_set.view + d + VIEW_COUNT) % VIEW_COUNT; break;
    case I_BGVIDEO: g_set.bg_video = !g_set.bg_video; media_started = false; break;
    case I_SHOWDISC: g_set.show_disc = !g_set.show_disc; break;
    case I_LAUNCHANIM: g_set.launch_anim = !g_set.launch_anim; break;
    case I_GAMEMUSIC: g_set.game_music = !g_set.game_music; media_started = false; break;
    case I_MENUMUSIC: g_set.menu_music = !g_set.menu_music; media_started = false; break;
    case I_SFX: g_set.sfx = !g_set.sfx; break;
    case I_VOLUME: g_set.volume = g_set.volume + d < 0 ? 0 : g_set.volume + d > 10 ? 10 : g_set.volume + d; audio_volume(g_set.volume); break;
    case I_SORT: { game_t *g = cur_game(); g_set.sort = (g_set.sort + d + SORT_COUNT) % SORT_COUNT; rebuild_view(g ? g->id : NULL); break; }
    case I_CLOCK24: g_set.clock24 = !g_set.clock24; break;
    case I_CLOCKADJ: g_set.clock_offset += d * 15; if (g_set.clock_offset > 24 * 60) g_set.clock_offset = 24 * 60; if (g_set.clock_offset < -24 * 60) g_set.clock_offset = -24 * 60; break;
    case I_SAVER: { int k = 0; for (int j = 0; j < 7; j++) if (savers[j] == g_set.screensaver) k = j;
        k = (k + d + 7) % 7; g_set.screensaver = savers[k]; break; }
    }
}

static void update_settings(uint32_t p) {
    int d = 0;
    if (p & BTN_UP) d = -1;
    if (p & BTN_DOWN) d = 1;
    if (d) {
        int n = set_cur;
        do { n = (n + d + I_COUNT) % I_COUNT; } while (is_header(n));
        set_cur = n;
        audio_sfx(SFX_MOVE);
    }
    if (p & (BTN_LEFT | BTN_RIGHT)) { change_item(set_cur, p & BTN_LEFT ? -1 : 1); audio_sfx(SFX_MOVE); }
    if (p & BTN_A) {
        if (set_cur == I_OPENSWISS) { audio_sfx(SFX_SELECT); open_swiss(); }
        else if (set_cur == I_RESET) {
            settings_defaults(); theme_set(0); lang_set(0); audio_volume(g_set.volume);
            ui_toast(tr(S_RESET_DEFAULTS)); audio_sfx(SFX_SELECT);
        } else { change_item(set_cur, 1); audio_sfx(SFX_MOVE); }
    }
    if (p & (BTN_B | BTN_START)) {
        if (fs_writable()) ui_toast(tr(settings_save() ? S_SAVED : S_SAVE_FAILED));
        screen = SCR_LIBRARY;
        audio_sfx(SFX_BACK);
    }
}

static void draw_settings(void) {
    ui_background(g_set.bg_video ? 0.35f : 0);
    ui_topbar(tr(S_SETTINGS), NULL);
    float x = 70, w = 500, y0 = 82, rh = 24, vis = 13.5f;
    ui_panel(x - 16, y0 - 12, w + 32, vis * rh + 18, 1.2f);
    float target = set_cur - 6;
    if (target > I_COUNT - vis) target = I_COUNT - vis;
    if (target < 0) target = 0;
    set_scroll += (target - set_scroll) * 0.25f;
    char buf[64];
    for (int i = 0; i < I_COUNT; i++) {
        float y = y0 + (i - set_scroll) * rh;
        if (y < y0 - 8 || y > y0 + (vis - 1) * rh) continue;
        if (is_header(i)) {
            draw_text(&g_font_s, x, y + 3, th->accent, tr(item_label(i)));
            draw_rect(x + text_width(&g_font_s, tr(item_label(i))) + 10, y + 13, w - text_width(&g_font_s, tr(item_label(i))) - 10, 1, col_alpha(th->accent, 0.35f));
            continue;
        }
        bool sel = i == set_cur;
        if (sel) draw_round_rect(x - 8, y, w + 16, rh - 2, 8, col_alpha(th->accent, 0.95f));
        uint32_t tc = sel ? 0x14102EFF : th->text;
        draw_text(&g_font_s, x + 10, y + 2, tc, tr(item_label(i)));
        const char *v = item_value(i, buf, sizeof buf);
        if (i == I_THEME) {    /* a swatch of the console colour */
            float sx = x + w - text_width(&g_font_s, v) - (sel ? 58 : 46);
            draw_round_rect(sx, y + 4, 22, 14, 4, g_themes[g_set.theme].console);
        }
        if (v && *v) {
            draw_text_right(&g_font_s, x + w - 14, y + 2, sel ? 0x14102EFF : th->text_hi, v);
            if (sel && i != I_OPENSWISS && i != I_RESET) {      /* < > arrows */
                float ax = x + w - 4, ay = y + rh / 2 - 1, vx = x + w - 22 - text_width(&g_font_s, v);
                float r1[8] = {ax, ay - 5, ax + 6, ay, ax + 6, ay, ax, ay + 5};
                float l1[8] = {vx, ay - 5, vx, ay - 5, vx - 6, ay, vx, ay + 5};
                uint32_t ac[4] = {0x14102EFF, 0x14102EFF, 0x14102EFF, 0x14102EFF};
                draw_quad_flat(r1, ac);
                draw_quad_flat(l1, ac);
            }
        }
    }
    static const int gl[] = {G_DPAD, G_A, G_B};
    const char *lb[] = {tr(S_CHANGE), tr(set_cur == I_OPENSWISS || set_cur == I_RESET ? S_PLAY : S_CHANGE), tr(S_SAVE)};
    ui_hints(gl, lb, 3);
}

/* ---------------- system ---------------- */

static void update_system(uint32_t p) {
    if (p & BTN_A) { audio_sfx(SFX_SELECT); open_swiss(); }
    if (p & BTN_Y) { audio_sfx(SFX_OPEN); screen = SCR_MEMCARD; screen_t = 0; mc_game = -1; mc_load(0); }
    if (p & (BTN_B | BTN_START)) { screen = SCR_LIBRARY; audio_sfx(SFX_BACK); }
}

static void draw_system(void) {
    ui_background(0);
    ui_topbar(tr(S_SYSTEM), NULL);
    float x = 54, y = 86;
    ui_panel(x - 18, y - 14, 272, 300, 1.2f);
    draw_tex(&g_tex_logo, x + 40, y - 4, 160, 144, 0xFFFFFFFF);
    y += 150;
    char buf[64];
    kv(x, y, tr(S_VERSION), CUBEMENU_VERSION); y += 22;
    kv(x, y, tr(S_DEVICE), fs_label()); y += 22;
    const char *sw = boot_find_swiss();
    kv(x, y, tr(S_SWISS_PATH), sw ? strchr(sw, ':') + 1 : tr(S_NOT_FOUND)); y += 22;
    snprintf(buf, sizeof buf, "%d", g_ngames);
    kv(x, y, tr(S_GAMES), buf); y += 22;
    uint32_t total = 0;
    for (int i = 0; i < g_ngames; i++) total += g_games[i].playtime;
    fmt_playtime(buf, sizeof buf, total);
    kv(x, y, tr(S_TOTAL_PLAYTIME), buf); y += 22;
    snprintf(buf, sizeof buf, "%u KB", (unsigned)(SYS_GetArenaHi() - SYS_GetArenaLo()) / 1024);
    kv(x, y, tr(S_FREE_MEM), buf);

    /* most played */
    float rx = 336, ry = 86;
    ui_panel(rx - 18, ry - 14, 286, 300, 1.2f);
    draw_text(&g_font_m, rx, ry, th->text_hi, tr(S_MOST_PLAYED));
    int all[MAX_GAMES];
    int n = library_view(all, FILTER_ALL, SORT_PLAYTIME);
    uint32_t top = n ? g_games[all[0]].playtime : 0;
    int shown = 0;
    for (int i = 0; i < n && shown < 6; i++) {
        game_t *g = &g_games[all[i]];
        if (!g->playtime) break;
        float yy = ry + 40 + shown * 40;
        draw_text_fit(&g_font_s, rx, yy, 172, th->text, g->name);
        fmt_playtime(buf, sizeof buf, g->playtime);
        draw_text_right(&g_font_s, rx + 250, yy, th->text_dim, buf);
        float bw = 250 * (top ? (float)g->playtime / top : 0);
        draw_round_rect(rx, yy + 21, 250, 6, 3, col_alpha(th->text_hi, 0.12f));
        draw_round_rect(rx, yy + 21, bw < 6 ? 6 : bw, 6, 3, th->accent);
        shown++;
    }
    if (!shown) draw_text_wrap(&g_font_s, rx, ry + 44, 250, 20, 0, 6, th->text_dim, tr(S_NEVER_PLAYED));
    draw_text_center(&g_font_s, SCREEN_W / 2, 398, th->text_dim, tr(S_ABOUT_LINE));
    static const int gl[] = {G_A, G_Y, G_B};
    const char *lb[] = {tr(S_OPEN_SWISS), tr(S_MEMCARDS), tr(S_BACK)};
    ui_hints(gl, lb, 3);
}

/* ---------------- memory cards ---------------- */

#define MAX_SAVES 128
static save_t saves[MAX_SAVES];
static int nsaves, mc_slot, mc_free, mc_cur;
static float mc_scroll;
static char mce_sent[7];          /* last game the GCMCE was switched to */

static const char *mc_title(void) { return mc_game >= 0 ? g_games[mc_game].name : tr(S_MEMCARDS); }

static void mc_msg(const char *msg) {
    gfx_begin(th->bg_top);
    ui_background(0);
    ui_topbar(mc_title(), NULL);
    draw_text_center(&g_font_m, SCREEN_W / 2, 220, th->text_dim, msg);
    gfx_end();
}

/* tries: a GCMCE that was just switched is away for a second or two (longer when it creates the card) */
static void mc_scan(int slot, int tries) {
    if (nsaves > 0) memcard_free(saves, nsaves);
    mc_msg(tr(S_READING));
    mc_slot = slot;
    while ((nsaves = memcard_scan(slot, saves, MAX_SAVES, &mc_free)) < 0 && --tries > 0) usleep(500000);
    if (mc_game >= 0 && nsaves > 0) {
        int k = 0;
        for (int i = 0; i < nsaves; i++) {
            if (!strncmp(saves[i].gamecode, g_games[mc_game].id, 4)) saves[k++] = saves[i];
            else memcard_free(&saves[i], 1);
        }
        nsaves = k;
    }
    mc_cur = 0;
    mc_scroll = 0;
}

static void mc_load(int slot) { mc_scan(slot, 1); }

/* the game's saves: a GCMCE / MemCard PRO is first switched to that game's virtual card */
static void mc_open_game(int gi) {
    mc_game = gi;
    screen = SCR_MEMCARD;
    screen_t = 0;
    int slot = memcard_is_mce(0) ? 0 : memcard_is_mce(1) ? 1 : -1;
    if (slot >= 0 && strcmp(mce_sent, g_games[gi].id)) {
        mc_msg(tr(S_MCE_SWITCHING));
        if (memcard_mce_set_game(slot, g_games[gi].id)) {
            snprintf(mce_sent, sizeof mce_sent, "%s", g_games[gi].id);
            usleep(1500000);
            mc_scan(slot, 16);
            return;
        }
    }
    mc_scan(slot < 0 ? 0 : slot, 1);
}

static void update_memcard(uint32_t p) {
    if (p & (BTN_L | BTN_R | BTN_LEFT | BTN_RIGHT)) { audio_sfx(SFX_OPEN); mc_load(!mc_slot); }
    if (p & BTN_UP && mc_cur > 0) { mc_cur--; audio_sfx(SFX_MOVE); }
    if (p & BTN_DOWN && mc_cur < nsaves - 1) { mc_cur++; audio_sfx(SFX_MOVE); }
    if (p & BTN_B) { if (nsaves > 0) memcard_free(saves, nsaves); nsaves = 0; screen = mc_game >= 0 ? SCR_DETAILS : SCR_SYSTEM; audio_sfx(SFX_BACK); }
}

static void draw_memcard(void) {
    ui_background(0);
    ui_topbar(mc_title(), NULL);
    /* slot tabs */
    for (int s = 0; s < 2; s++) {
        float x = 54 + s * 130;
        bool on = s == mc_slot;
        draw_text(&g_font_m, x, 74, on ? th->text_hi : th->text_dim, tr(s ? S_SLOT_B : S_SLOT_A));
        if (on) draw_round_rect(x, 104, text_width(&g_font_m, tr(s ? S_SLOT_B : S_SLOT_A)), 3, 1, th->accent);
    }
    char buf[64];
    if (nsaves < 0) {
        draw_text_center(&g_font_m, SCREEN_W / 2, 220, th->text_dim, tr(S_NO_MEMCARD));
    } else if (nsaves == 0 && mc_game >= 0) {
        draw_text_center(&g_font_m, SCREEN_W / 2, 220, th->text_dim, tr(S_NO_GAME_SAVES));
    } else {
        int k = snprintf(buf, sizeof buf, tr(S_N_SAVES), nsaves);
        snprintf(buf + k, sizeof buf - k, "   ·   ");
        k = strlen(buf);
        snprintf(buf + k, sizeof buf - k, tr(S_FREE_BLOCKS), mc_free);
        draw_text_right(&g_font_s, SCREEN_W - 54, 82, th->text_dim, buf);
        const float x = 40, y0 = 122, rh = 44, vis = 6;
        float target = mc_cur - 2;
        if (target > nsaves - vis) target = nsaves - vis;
        if (target < 0) target = 0;
        mc_scroll += (target - mc_scroll) * 0.25f;
        ui_panel(x - 10, y0 - 8, SCREEN_W - 2 * x + 20, vis * rh + 12, 1.1f);
        for (int i = (int)mc_scroll; i < nsaves && i < mc_scroll + vis + 1; i++) {
            float y = y0 + (i - mc_scroll) * rh;
            if (y < y0 - 4 || y > y0 + (vis - 1) * rh + 4) continue;
            save_t *sv = &saves[i];
            bool sel = i == mc_cur;
            if (sel) draw_round_rect(x - 4, y - 2, SCREEN_W - 2 * x + 8, rh - 4, 8, col_alpha(th->accent, 0.9f));
            if (sv->icon.data) draw_tex(&sv->icon, x + 4, y + 3, 32, 32, 0xFFFFFFFF);
            else draw_tex(&g_tex_emblem, x + 6, y + 5, 28, 28, 0xFFFFFF70);
            uint32_t tc = sel ? 0x14102EFF : th->text_hi, dc = sel ? 0x2A2350FF : th->text_dim;
            draw_text_fit(&g_font_s, x + 46, y + 1, 330, tc, sv->title);
            draw_text_fit(&g_font_s, x + 46, y + 19, 330, dc, sv->sub[0] ? sv->sub : sv->file);
            /* the game's cover when it is in the library */
            for (int g = 0; g < g_ngames; g++) {
                if (strncmp(g_games[g].id, sv->gamecode, 4)) continue;
                const tex_t *c = assets_get(g, AS_CASE);
                if (c) draw_tex_uv(c, SCREEN_W - x - 150, y + 2, 25, 35, 0.5254f, 0, 1, 1, 0xFFFFFFFF);
                break;
            }
            time_t t = (time_t)sv->time + 946684800;
            struct tm *tm = gmtime(&t);
            snprintf(buf, sizeof buf, "%04d-%02d-%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
            draw_text_right(&g_font_s, SCREEN_W - x - 6, y + 1, dc, buf);
            snprintf(buf, sizeof buf, tr(S_BLOCKS), (int)sv->blocks);
            draw_text_right(&g_font_s, SCREEN_W - x - 6, y + 19, dc, buf);
        }
        ui_scrollbar(SCREEN_W - x + 14, y0, vis * rh - 8, nsaves > vis ? mc_scroll / (nsaves - vis) : 0, vis / (float)(nsaves ? nsaves : 1));
    }
    static const int gl[] = {G_LR, G_DPAD, G_B};
    const char *lb[] = {tr(S_SLOT), tr(S_SCROLL), tr(S_BACK)};
    ui_hints(gl, lb, 3);
}

/* ---------------- launch scene ---------------- */

#define LAUNCH_T_LID_OPEN 0.55f
#define LAUNCH_T_DISC 1.25f
#define LAUNCH_T_LID_CLOSE 2.35f
#define LAUNCH_T_LED 3.0f
#define LAUNCH_T_END 4.6f

static void draw_launch(float dt) {
    game_t *g = &g_games[launch_game];
    float t = launch_t;
    float fade_in = clampf(t / 0.45f, 0, 1);
    draw_rect_grad(0, 0, SCREEN_W, SCREEN_H, th->bg_top, th->bg_bottom);
    draw_glow(SCREEN_W / 2, 250, 300, col_alpha(th->accent2, 0.35f * fade_in));

    float lid = ease_io((t - LAUNCH_T_LID_OPEN) / 0.6f) * (1 - ease_io((t - LAUNCH_T_LID_CLOSE) / 0.5f));
    float dk = clampf((t - LAUNCH_T_DISC) / 0.9f, 0, 1);
    float disc_y = t < LAUNCH_T_DISC ? -1 : 150 * (1 - ease_out(dk));
    static float spin;
    spin += dt * (t > LAUNCH_T_LED ? 18.0f * clampf((t - LAUNCH_T_LED) / 0.8f, 0.1f, 1) : 1.2f);
    float led = clampf((t - LAUNCH_T_LED) / 0.15f, 0, 1);

    /* camera: a three-quarter view from the side, up over the top while the disc goes in, then down
     * again to the emblem on the closed lid */
    float up = ease_io((t - (LAUNCH_T_LID_OPEN - 0.1f)) / 0.7f) * (1 - ease_io((t - LAUNCH_T_LID_CLOSE) / 0.8f));
    float yaw = lerpf(40, -34, ease_io(t / LAUNCH_T_END)) * (1 - 0.6f * up);
    float elev = lerpf(28, 66, up);
    float dist = lerpf(560, 470, up) - 40 * ease_io((t - LAUNCH_T_LED) / 1.4f) + 60 * (1 - ease_out(fade_in));
    vec3 at = {0, lerpf(48, 70, up), lerpf(0, -10, up)};
    float yr = yaw * (float)M_PI / 180, er = elev * (float)M_PI / 180;
    vec3 eye = {at.x + dist * sinf(yr) * cosf(er), at.y + dist * sinf(er), at.z + dist * cosf(yr) * cosf(er)};
    gfx_3d_shift(34, eye, at, 46);
    console3d(0, assets_get(launch_game, AS_DISC), lid, disc_y, spin, led, 1);
    gfx_2d();
    if (fade_in < 1) draw_rect(0, 0, SCREEN_W, SCREEN_H, col_alpha(th->bg_top, 1 - fade_in));   /* solid model, faded in */

    float ca = clampf((t - 0.3f) / 0.4f, 0, 1);
    draw_text_center(&g_font_l, SCREEN_W / 2 + 1, 384, col_alpha(0x00000080, ca), g->name);
    draw_text_center(&g_font_l, SCREEN_W / 2, 382, col_alpha(th->text_hi, ca), g->name);
    char s[48];
    int dots = ((int)(t * 3)) % 4;
    snprintf(s, sizeof s, "%s%.*s", tr(S_LAUNCHING), dots, "...");
    if (g->ndisc > 1) snprintf(s + strlen(s), sizeof s - strlen(s), "   %s %d", tr(S_DISC), launch_disc + 1);
    draw_text(&g_font_s, SCREEN_W / 2 - text_width(&g_font_s, tr(S_LAUNCHING)) / 2, 420, col_alpha(th->text_dim, ca), s);
    float bw = 280, bx = (SCREEN_W - bw) / 2, by = 446, pr = clampf(t / (LAUNCH_T_END - 0.2f), 0, 1);
    draw_round_rect(bx, by, bw, 6, 3, col_alpha(0xFFFFFFFF, 0.15f * ca));
    draw_round_rect(bx, by, fmaxf(6, bw * pr), 6, 3, col_alpha(th->accent, ca));
    /* end: white flash into black, like the console handing over to the disc */
    float fl = clampf((t - (LAUNCH_T_END - 0.35f)) / 0.2f, 0, 1);
    float fo = clampf((t - (LAUNCH_T_END - 0.15f)) / 0.3f, 0, 1);
    if (fl > 0) draw_rect(0, 0, SCREEN_W, SCREEN_H, col_alpha(0xFFFFFFFF, fl * (1 - fo)));
    if (fo > 0) draw_rect(0, 0, SCREEN_W, SCREEN_H, col_alpha(0x000000FF, fo));
}

/* ---------------- screensaver ---------------- */

static void draw_saver(void) {
    float a = clampf(saver_t / 1.0f, 0, 1);
    draw_rect(0, 0, SCREEN_W, SCREEN_H, col_alpha(0x000000FF, 0.92f * a));
    float t = saver_t;
    float x = 120 + fabsf(fmodf(t * 37, 800) - 400) / 400 * 360, y = 80 + fabsf(fmodf(t * 23, 520) - 260) / 260 * 250;
    draw_glow(x + 50, y + 50, 120, col_alpha(th->accent2, 0.25f * a));
    draw_tex(&g_tex_emblem, x, y, 100, 100, col_alpha(0xFFFFFFFF, 0.9f * a));
    char clk[16];
    ui_clock_string(clk, sizeof clk);
    draw_text_center(&g_font_xl, SCREEN_W / 2, 380, col_alpha(th->text_dim, 0.6f * a), clk);
}

/* ---------------- frame ---------------- */

void app_init(void) {
    card_ok = fs_root()[0] != 0;      /* mounted in main() */
    /* escape hatch: B held at power on goes straight to Swiss */
    VIDEO_WaitVSync();
    PAD_ScanPads();
    if (card_ok && (PAD_ButtonsHeld(0) & PAD_BUTTON_B)) {
        const char *swiss = boot_find_swiss();
        char err[96];
        if (swiss) boot_dol(swiss, 1, NO_AUTOLOAD, before_jump, err, sizeof err);
    }
    trace("settings...");
    settings_load();
    theme_set(g_set.theme);
    lang_set(g_set.lang);
    trace("library...");
    if (card_ok) { library_scan(); trace("library: %d games", g_ngames); stats_load(); }
    trace("texture loader...");
    assets_init();
    trace("clip player...");
    clip_init();
    trace("audio...");
    audio_init();
    audio_volume(g_set.volume);
    trace("audio: ok");
    rebuild_view(g_set.last_game[0] ? g_set.last_game : NULL);
    demo_load();
    trace_done();
    pending_toast = stats_credit_message();       /* "+1h 20m  Pikmin": shown once the intro is over */
}

void app_frame(float dt) {
    u64 tf0 = gettime();
    input_update();
    demo_step();
    uint32_t p = input_pressed();
    ui_tick(dt);
    audio_update(dt);
    screen_t += dt;

    /* screensaver: any button wakes it and is swallowed */
    if (saver) {
        saver_t += dt;
        if (input_held()) { saver = false; p = 0; }
    } else if (g_set.screensaver && screen == SCR_LIBRARY && input_idle_frames() > g_set.screensaver * 60 * 60 && !app_demo_active()) {
        saver = true; saver_t = 0;
    }

    /* smooth cursor */
    pos_s += (cur - pos_s) * clampf(dt * 11, 0, 1);
    if (fabsf(cur - pos_s) < 0.002f) pos_s = cur;
    sel_t += dt;
    sidebar_a = clampf(sidebar_a + (sidebar ? dt : -dt) * 5, 0, 1);
    if (nview && screen != SCR_LAUNCH && screen != SCR_INTRO && !media_started && sel_t > 0.55f) start_media();
    if (nview) assets_focus(view, nview, cur, g_set.view == VIEW_GRID ? 18 : 9);

    switch (screen) {
    case SCR_INTRO:
        if ((p & (BTN_A | BTN_START | BTN_B)) || screen_t > 3.2f) {
            screen = SCR_LIBRARY; screen_t = 0; sel_t = 0; media_started = false;
            if (pending_toast) { ui_toast(pending_toast); pending_toast = NULL; }
        }
        break;
    case SCR_LIBRARY: if (!saver) update_library(p, dt); break;
    case SCR_DETAILS: update_details(p); break;
    case SCR_SETTINGS: update_settings(p); break;
    case SCR_SYSTEM: update_system(p); break;
    case SCR_MEMCARD: update_memcard(p); break;
    case SCR_LAUNCH:
        launch_t += dt;
        if (launch_t >= LAUNCH_T_END + 0.2f) do_boot();
        break;
    }

    u64 tf1 = gettime();
    gfx_begin(th->bg_top);
    switch (screen) {
    case SCR_INTRO: draw_intro(); break;
    case SCR_LIBRARY: draw_library(); break;
    case SCR_DETAILS: draw_details(); break;
    case SCR_SETTINGS: draw_settings(); break;
    case SCR_SYSTEM: draw_system(); break;
    case SCR_MEMCARD: draw_memcard(); break;
    case SCR_LAUNCH: draw_launch(dt); break;
    }
    if (saver) draw_saver();
    ui_toast_draw();
    if (demo && demo_fps) {   /* frame time, emulated seconds (perf checks) */
        static float acc; static int frames; static char fps[24];
        acc += dt; frames++;
        if (acc >= 0.5f) { snprintf(fps, sizeof fps, "%.1f fps", frames / acc); acc = 0; frames = 0; }
        extern float g_ms_gpu, g_ms_vsync;
        static float upd, drw;
        u64 tf2 = gettime();
        upd = upd * 0.9f + 0.1f * ticks_to_microsecs(diff_ticks(tf0, tf1)) / 1000.0f;
        drw = drw * 0.9f + 0.1f * ticks_to_microsecs(diff_ticks(tf1, tf2)) / 1000.0f;
        char vm[96];
        snprintf(vm, sizeof vm, "%s u%.1f d%.1f g%.1f v%.1f", fps, upd, drw, g_ms_gpu, g_ms_vsync);
        draw_rect(4, 4, 330, 22, 0x000000C0);
        draw_text(&g_font_s, 8, 5, 0xFFFF00FF, vm);
    }
    if (demo) {   /* tour recordings: the app frame number as a 20-bit strip on the last 4 rows */
        for (int b = 0; b < 20; b++)
            draw_rect(b * 32, SCREEN_H - 4, 32, 4, (demo_frame >> b) & 1 ? 0xFFFFFFFF : 0x000000FF);
    }
    gfx_end();
}
