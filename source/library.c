#include "library.h"
#include "fs.h"
#include "ini.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

game_t g_games[MAX_GAMES];
int g_ngames;

static const char *LANG_KEYS[NLANG] = {"synopsis.pt", "synopsis.en", "synopsis.es"};

static bool ends_with_ci(const char *s, const char *suf) {
    size_t a = strlen(s), b = strlen(suf);
    return a >= b && !strcasecmp(s + a - b, suf);
}

/* "Mario Party 4 (v1.01)" -> "Mario Party 4" */
static void display_name(char *dst, int n, const char *folder) {
    snprintf(dst, n, "%s", folder);
    char *p = strrchr(dst, '(');
    if (p && (!strncasecmp(p, "(v", 2) || !strncasecmp(p, "(rev", 4) || !strncasecmp(p, "(player", 7))) {
        while (p > dst && p[-1] == ' ') p--;
        *p = 0;
    }
}

static bool read_header(const char *path, char id[7], int *disc) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    uint8_t h[0x20];
    bool ok = fread(h, 1, sizeof h, f) == sizeof h;
    fclose(f);
    if (!ok || h[0x1C] != 0xC2 || h[0x1D] != 0x33 || h[0x1E] != 0x9F || h[0x1F] != 0x3D) return false;
    for (int i = 0; i < 6; i++) { if (!isalnum(h[i])) return false; id[i] = h[i]; }
    id[6] = 0;
    *disc = h[6];
    return true;
}

static void load_meta(game_t *g) {
    char rel[96], path[160];
    game_asset_path(rel, sizeof rel, g, "info.txt");
    fs_path(path, sizeof path, rel);
    char *txt = fs_read_all(path, NULL);
    if (!txt) return;
    ini_t ini;
    ini_parse(&ini, txt);
    const char *v;
    if ((v = ini_get(&ini, "title")) && *v) snprintf(g->name, sizeof g->name, "%s", v);
    if ((v = ini_get(&ini, "developer"))) snprintf(g->developer, sizeof g->developer, "%s", v);
    if ((v = ini_get(&ini, "publisher"))) snprintf(g->publisher, sizeof g->publisher, "%s", v);
    if ((v = ini_get(&ini, "genre"))) snprintf(g->genre, sizeof g->genre, "%s", v);
    if ((v = ini_get(&ini, "players"))) snprintf(g->players, sizeof g->players, "%s", v);
    if ((v = ini_get(&ini, "rating"))) snprintf(g->rating, sizeof g->rating, "%s", v);
    if ((v = ini_get(&ini, "year"))) g->year = atoi(v);
    g->has_clip = ini_get_int(&ini, "clip", 0);
    g->has_music = ini_get_int(&ini, "music", 0);
    g->has_shot[0] = ini_get_int(&ini, "snap", 0);
    g->has_shot[1] = ini_get_int(&ini, "title_shot", 0);
    for (int l = 0; l < NLANG; l++) {
        if ((v = ini_get(&ini, LANG_KEYS[l])) && *v) {
            g->synopsis[l] = strdup(v);
            for (char *p = g->synopsis[l]; *p; p++)        /* "\n" escapes back to newlines */
                if (p[0] == '\\' && p[1] == 'n') { p[0] = ' '; p[1] = '\n'; }
        }
    }
    free(txt);
}

static int cmp_disc(const void *a, const void *b) { return strcasecmp(a, b); }

void library_scan(void) {
    char path[256];
    g_ngames = 0;
    DIR *d = opendir(fs_path(path, sizeof path, "/games"));
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && g_ngames < MAX_GAMES) {
        if (e->d_name[0] == '.') continue;
        char folder[160];
        snprintf(folder, sizeof folder, "/games/%s", e->d_name);
        DIR *sd = opendir(fs_path(path, sizeof path, folder));
        if (!sd) continue;
        game_t *g = &g_games[g_ngames];
        memset(g, 0, sizeof *g);
        char names[MAX_DISCS][40];
        int discno[MAX_DISCS], n = 0;
        struct dirent *f;
        while ((f = readdir(sd)) && n < MAX_DISCS) {
            if (f->d_name[0] == '.' || !(ends_with_ci(f->d_name, ".iso") || ends_with_ci(f->d_name, ".gcm"))) continue;
            if (strlen(f->d_name) >= sizeof names[0]) continue;
            char full[256], id[7];
            int disc;
            snprintf(full, sizeof full, "%s/%s", path, f->d_name);
            if (!read_header(full, id, &disc)) continue;
            if (!n) memcpy(g->id, id, 7);
            else if (strcmp(g->id, id)) continue;      /* another game in the same folder: skip */
            snprintf(names[n], sizeof names[n], "%s", f->d_name);
            discno[n++] = disc;
        }
        closedir(sd);
        if (!n) continue;
        /* order discs by the disc number in the header */
        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++)
                if (discno[j] < discno[i] || (discno[j] == discno[i] && cmp_disc(names[j], names[i]) < 0)) {
                    char t[40]; int td = discno[i];
                    memcpy(t, names[i], 40); memcpy(names[i], names[j], 40); memcpy(names[j], t, 40);
                    discno[i] = discno[j]; discno[j] = td;
                }
        for (int i = 0; i < n; i++) memcpy(g->disc[i], names[i], 40);
        g->ndisc = n;
        snprintf(g->folder, sizeof g->folder, "%s", folder);
        display_name(g->name, sizeof g->name, e->d_name);
        load_meta(g);
        g_ngames++;
    }
    closedir(d);
}

const char *game_synopsis(const game_t *g, int lang) {
    if (lang >= 0 && lang < NLANG && g->synopsis[lang]) return g->synopsis[lang];
    if (g->synopsis[1]) return g->synopsis[1];
    for (int l = 0; l < NLANG; l++) if (g->synopsis[l]) return g->synopsis[l];
    return NULL;
}

void game_asset_path(char *buf, int n, const game_t *g, const char *file) {
    snprintf(buf, n, "/cubemenu/games/%s/%s", g->id, file);
}

int game_launch_path(char *buf, int n, const game_t *g, int disc) {
    if (disc < 0 || disc >= g->ndisc) disc = 0;
    return snprintf(buf, n, "%s:%s/%s", fs_root(), g->folder, g->disc[disc]);
}

/* ---------------- stats ---------------- */

static char credit_msg[96];

void fmt_playtime(char *buf, int n, uint32_t s) {
    if (s < 60) snprintf(buf, n, "%um", s ? 1u : 0u);
    else if (s < 3600) snprintf(buf, n, "%um", s / 60);
    else snprintf(buf, n, "%uh %02um", s / 3600, (s / 60) % 60);
}

void stats_load(void) {
    char path[96];
    char *txt = fs_read_all(fs_path(path, sizeof path, "/cubemenu/stats.ini"), NULL);
    if (!txt) return;
    ini_t ini;
    ini_parse(&ini, txt);
    for (int i = 0; i < g_ngames; i++) {
        game_t *g = &g_games[i];
        const char *v = ini_get(&ini, g->id);
        if (!v) continue;
        unsigned pt = 0, ln = 0, lp = 0, fav = 0;
        sscanf(v, "%u,%u,%u,%u", &pt, &ln, &lp, &fav);
        g->playtime = pt; g->launches = ln; g->last_played = lp; g->favorite = fav;
    }
    /* credit the last session */
    const char *pending = ini_get(&ini, "pending");
    if (pending && *pending) {
        char id[8] = {0};
        unsigned t0 = 0;
        if (sscanf(pending, "%6[A-Z0-9],%u", id, &t0) == 2) {
            time_t now = time(NULL);
            for (int i = 0; i < g_ngames; i++) {
                if (strcmp(g_games[i].id, id)) continue;
                long dt = (long)now - (long)t0;
                if (dt > 30 && dt < 12 * 3600) {     /* ignore clock jumps and forgotten consoles */
                    g_games[i].playtime += dt;
                    char pt[24];
                    fmt_playtime(pt, sizeof pt, dt);
                    snprintf(credit_msg, sizeof credit_msg, "+%s  %s", pt, g_games[i].name);
                }
                break;
            }
        }
        free(txt);
        stats_save();     /* clears 'pending' */
        return;
    }
    free(txt);
}

static char pending_line[32];

void stats_save(void) {
    int cap = 64 + g_ngames * 48;
    char *buf = malloc(cap), *p = buf;
    p += sprintf(p, "# Cube Menu stats: playtime,launches,last_played,favorite\n");
    if (pending_line[0]) p += sprintf(p, "pending=%s\n", pending_line);
    for (int i = 0; i < g_ngames; i++) {
        game_t *g = &g_games[i];
        if (!g->playtime && !g->launches && !g->favorite) continue;
        p += sprintf(p, "%s=%u,%u,%u,%u\n", g->id, (unsigned)g->playtime, (unsigned)g->launches, (unsigned)g->last_played, g->favorite ? 1u : 0u);
    }
    char path[96];
    fs_write_all(fs_path(path, sizeof path, "/cubemenu/stats.ini"), buf, p - buf);
    free(buf);
}

void stats_mark_launch(game_t *g) {
    g->launches++;
    g->last_played = time(NULL);
    snprintf(pending_line, sizeof pending_line, "%s,%u", g->id, (unsigned)g->last_played);
    stats_save();
}

const char *stats_credit_message(void) { return credit_msg[0] ? credit_msg : NULL; }

/* ---------------- views ---------------- */

static int sort_mode;
static int cmp_games(const void *a, const void *b) {
    const game_t *x = &g_games[*(const int *)a], *y = &g_games[*(const int *)b];
    switch (sort_mode) {
    case SORT_RECENT: if (x->last_played != y->last_played) return x->last_played > y->last_played ? -1 : 1; break;
    case SORT_PLAYTIME: if (x->playtime != y->playtime) return x->playtime > y->playtime ? -1 : 1; break;
    case SORT_YEAR: if (x->year != y->year) return x->year < y->year ? -1 : 1; break;
    }
    return strcasecmp(x->name, y->name);
}

int library_view(int *out, int filter, int sort) {
    int n = 0;
    for (int i = 0; i < g_ngames; i++) {
        if (filter == FILTER_FAV && !g_games[i].favorite) continue;
        if (filter == FILTER_RECENT && !g_games[i].last_played) continue;
        out[n++] = i;
    }
    sort_mode = filter == FILTER_RECENT ? SORT_RECENT : sort;
    qsort(out, n, sizeof *out, cmp_games);
    return n;
}
