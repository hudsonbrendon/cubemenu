#include "library.h"
#include "fs.h"
#include "ini.h"
#include "trace.h"
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
    g->has_manual = ini_get_int(&ini, "manual", 0);
    static const char *const HL[3] = {"hltb_main", "hltb_plus", "hltb_all"};
    for (int i = 0; i < 3; i++) g->hltb[i] = ini_get_int(&ini, HL[i], 0);
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

/* ---------------- homebrew in /apps ---------------- */

static const char *app_main;      /* folder name, for cmp_exe */
static int exe_rank(const char *f) {
    size_t n = strlen(app_main);
    return !strncasecmp(f, app_main, n) && !strcasecmp(f + n, ".dol") ? 0 : 1;
}
static int cmp_exe(const void *a, const void *b) {
    int ra = exe_rank(a), rb = exe_rank(b);
    if (ra != rb) return ra - rb;
    size_t la = strlen(a), lb = strlen(b);
    return la != lb ? (int)la - (int)lb : strcasecmp(a, b);
}

/* "~" + up to 5 letters of the folder: a stats / assets key that no disc ID can have */
static void app_id(char id[7], const char *folder) {
    int k = 0;
    id[k++] = '~';
    for (const char *p = folder; *p && k < 6; p++) if (isalnum((unsigned char)*p)) id[k++] = toupper((unsigned char)*p);
    id[k] = 0;
    for (int tries = 0; tries < 10; tries++) {
        bool dup = false;
        for (int i = 0; i < g_ngames; i++) if (!strcmp(g_games[i].id, id)) dup = true;
        if (!dup) return;
        id[k < 6 ? k++ : 5] = '0' + tries;
        id[k] = 0;
    }
}

static void scan_apps(void) {
    char path[256];
    DIR *d = opendir(fs_path(path, sizeof path, "/apps"));
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && g_ngames < MAX_GAMES) {
        if (e->d_name[0] == '.') continue;
        char folder[160];
        snprintf(folder, sizeof folder, "/apps/%s", e->d_name);
        DIR *sd = opendir(fs_path(path, sizeof path, folder));
        if (!sd) continue;
        char (*exe)[48] = malloc(32 * sizeof *exe);
        int n = 0;
        struct dirent *f;
        while ((f = readdir(sd)) && n < 32) {
            if (f->d_name[0] == '.' || strlen(f->d_name) >= sizeof exe[0]) continue;
            if (ends_with_ci(f->d_name, ".dol") || ends_with_ci(f->d_name, ".dol+cli")) snprintf(exe[n++], sizeof exe[0], "%s", f->d_name);
        }
        closedir(sd);
        if (!n) { free(exe); continue; }
        app_main = e->d_name;
        qsort(exe, n, sizeof *exe, cmp_exe);
        game_t *g = &g_games[g_ngames];
        memset(g, 0, sizeof *g);
        g->is_app = true;
        g->exe = exe;
        g->nexe = n;
        app_id(g->id, e->d_name);
        snprintf(g->folder, sizeof g->folder, "%s", folder);
        snprintf(g->name, sizeof g->name, "%s", e->d_name);
        load_meta(g);
        g_ngames++;
    }
    closedir(d);
}

const char *app_variant(const game_t *g, int exe) {
    static char buf[48];
    if (exe <= 0 || exe >= g->nexe) return NULL;
    const char *f = g->exe[exe], *slash = strrchr(g->folder, '/');
    const char *base = slash ? slash + 1 : g->folder;
    size_t n = strlen(base);
    if (!strncasecmp(f, base, n) && (f[n] == '-' || f[n] == '_')) f += n + 1;
    snprintf(buf, sizeof buf, "%s", f);
    char *dot = strstr(buf, ".dol");
    if (!dot) dot = strstr(buf, ".DOL");
    if (dot) *dot = 0;
    return buf;
}

/* end of the DOL image: the highest section end in its header */
static long dol_size(const uint8_t *h) {
    long end = 0x100;
    for (int i = 0; i < 18; i++) {
        uint32_t off = h[i * 4] << 24 | h[i * 4 + 1] << 16 | h[i * 4 + 2] << 8 | h[i * 4 + 3];
        uint32_t sz = h[0x90 + i * 4] << 24 | h[0x91 + i * 4] << 16 | h[0x92 + i * 4] << 8 | h[0x93 + i * 4];
        if (sz && off + sz > end) end = off + sz;
    }
    return end;
}

int app_launch(const game_t *g, int exe, char *path, int n, const char **args, int maxargs, char **store) {
    if (exe < 0 || exe >= g->nexe) exe = 0;
    char rel[256];
    snprintf(rel, sizeof rel, "%s/%s", g->folder, g->exe[exe]);
    fs_path(path, n, rel);
    *store = NULL;
    /* the arguments: X.cli next to X.dol, or the text after the DOL in X.dol+cli */
    char *txt = NULL;
    if (ends_with_ci(rel, ".dol")) {
        char cli[260];
        snprintf(cli, sizeof cli, "%.*s.cli", (int)strlen(path) - 4, path);
        txt = fs_read_all(cli, NULL);
    } else {
        int len = 0;
        uint8_t *dol = fs_read_all(path, &len);
        if (dol && len > 0x100) {
            long end = dol_size(dol);
            if (end < len) { txt = strdup((char *)dol + end); }
        }
        free(dol);
    }
    if (!txt) return 0;
    *store = txt;
    int k = 0;
    for (char *line = strtok(txt, "\r\n"); line && k < maxargs; line = strtok(NULL, "\r\n")) {
        while (*line == ' ' || *line == '\t') line++;
        if (!*line || *line == '#') continue;
        args[k++] = line;
    }
    return k;
}

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
    scan_apps();
}

const char *game_synopsis(const game_t *g, int lang) {
    if (lang >= 0 && lang < NLANG && g->synopsis[lang]) return g->synopsis[lang];
    if (g->synopsis[1]) return g->synopsis[1];
    for (int l = 0; l < NLANG; l++) if (g->synopsis[l]) return g->synopsis[l];
    return NULL;
}

static const char *const GENRES[][4] = {   /* GameTDB name, pt, en, es */
    {"action", "Ação", "Action", "Acción"},
    {"adventure", "Aventura", "Adventure", "Aventura"},
    {"racing", "Corrida", "Racing", "Carreras"},
    {"kart racing", "Corrida de kart", "Kart racing", "Carreras de karts"},
    {"futuristic racing", "Corrida futurista", "Futuristic racing", "Carreras futuristas"},
    {"party", "Party game", "Party game", "Juego de fiesta"},
    {"shooter", "Tiro", "Shooter", "Disparos"},
    {"first-person shooter", "Tiro em primeira pessoa", "First-person shooter", "Disparos en primera persona"},
    {"third-person shooter", "Tiro em terceira pessoa", "Third-person shooter", "Disparos en tercera persona"},
    {"beat 'em up", "Briga de rua", "Beat 'em up", "Beat 'em up"},
    {"survival horror", "Survival horror", "Survival horror", "Survival horror"},
    {"strategy", "Estratégia", "Strategy", "Estrategia"},
    {"turn-based strategy", "Estratégia por turnos", "Turn-based strategy", "Estrategia por turnos"},
    {"soccer", "Futebol", "Soccer", "Fútbol"},
    {"2d platformer", "Plataforma 2D", "2D platformer", "Plataformas 2D"},
    {"3d platformer", "Plataforma 3D", "3D platformer", "Plataformas 3D"},
    {"platformer", "Plataforma", "Platformer", "Plataformas"},
    {"role-playing", "RPG", "Role-playing", "Rol"},
    {"fighting", "Luta", "Fighting", "Lucha"},
    {"3d fighting", "Luta 3D", "3D fighting", "Lucha 3D"},
    {"golf", "Golfe", "Golf", "Golf"},
    {"tennis", "Tênis", "Tennis", "Tenis"},
    {"baseball", "Beisebol", "Baseball", "Béisbol"},
    {"sports", "Esportes", "Sports", "Deportes"},
    {"puzzle", "Quebra-cabeça", "Puzzle", "Puzle"},
    {"music", "Música", "Music", "Música"},
    {"rhythm", "Ritmo", "Rhythm", "Ritmo"},
    {"simulation", "Simulação", "Simulation", "Simulación"},
    {"emulator", "Emulador", "Emulator", "Emulador"},
    {"homebrew", "Homebrew", "Homebrew", "Homebrew"},
};

const char *game_genre(const game_t *g, int lang) {
    static char buf[40];
    if (!g->genre[0]) return "";
    for (unsigned i = 0; i < sizeof GENRES / sizeof *GENRES; i++)
        if (!strcasecmp(g->genre, GENRES[i][0])) return GENRES[i][1 + (lang >= 0 && lang < 3 ? lang : 1)];
    snprintf(buf, sizeof buf, "%s", g->genre);
    buf[0] = toupper((unsigned char)buf[0]);
    return buf;
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
static uint32_t ach_seen, ach_new;
static bool just_played;

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
        unsigned pt = 0, ln = 0, lp = 0, fav = 0, col = 0, best = 0;
        sscanf(v, "%u,%u,%u,%u,%u,%u", &pt, &ln, &lp, &fav, &col, &best);
        g->playtime = pt; g->launches = ln; g->last_played = lp; g->favorite = fav;
        g->collections = col; g->best_session = best;
    }
    const char *am = ini_get(&ini, "achievements");
    ach_seen = am ? (uint32_t)strtoul(am, NULL, 10) : 0;
    /* credit the last session */
    const char *pending = ini_get(&ini, "pending");
    if (pending && *pending) {
        char id[8] = {0};
        unsigned t0 = 0;
        if (sscanf(pending, "%6[^,],%u", id, &t0) == 2) {
            time_t now = time(NULL);
            for (int i = 0; i < g_ngames; i++) {
                if (strcmp(g_games[i].id, id)) continue;
                long dt = (long)now - (long)t0;
                just_played = dt >= 0 && dt < 6 * 3600;
                if (dt > 30 && dt < 12 * 3600) {     /* ignore clock jumps and forgotten consoles */
                    g_games[i].playtime += dt;
                    if ((uint32_t)dt > g_games[i].best_session) g_games[i].best_session = dt;
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
    if (achievements() != ach_seen) stats_save();
}

static char pending_line[32];

void stats_save(void) {
    int cap = 96 + g_ngames * 64;
    char *buf = malloc(cap), *p = buf;
    p += sprintf(p, "# Cube Menu stats: playtime,launches,last_played,favorite\n");
    if (pending_line[0]) p += sprintf(p, "pending=%s\n", pending_line);
    uint32_t ach = achievements();
    ach_new |= ach & ~ach_seen;
    ach_seen = ach;
    p += sprintf(p, "achievements=%u\n", (unsigned)ach);
    for (int i = 0; i < g_ngames; i++) {
        game_t *g = &g_games[i];
        if (!g->playtime && !g->launches && !g->favorite && !g->collections) continue;
        p += sprintf(p, "%s=%u,%u,%u,%u,%u,%u\n", g->id, (unsigned)g->playtime, (unsigned)g->launches, (unsigned)g->last_played,
                     g->favorite ? 1u : 0u, (unsigned)g->collections, (unsigned)g->best_session);
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

uint32_t achievements_new(void) { return ach_new; }
bool stats_just_played(void) { return just_played; }

uint32_t achievements(void) {
    uint32_t total = 0, best = 0, top = 0, m = 0;
    int played = 0, done = 0, favs = 0, ngenres = 0;
    char genres[16][40];
    for (int i = 0; i < g_ngames; i++) {
        const game_t *g = &g_games[i];
        total += g->playtime;
        if (g->best_session > best) best = g->best_session;
        if (g->playtime > top) top = g->playtime;
        if (g->launches && !g->is_app) played++;
        if (g->collections & COL_DONE) done++;
        if (g->favorite) favs++;
        if (g->launches && g->genre[0] && ngenres < 16) {
            bool seen = false;
            for (int k = 0; k < ngenres; k++) if (!strcasecmp(genres[k], g->genre)) seen = true;
            if (!seen) snprintf(genres[ngenres++], 40, "%s", g->genre);
        }
    }
    if (played) m |= 1u << ACH_FIRST;
    if (total >= 10 * 3600) m |= 1u << ACH_10H;
    if (total >= 50 * 3600) m |= 1u << ACH_50H;
    if (total >= 100 * 3600) m |= 1u << ACH_100H;
    if (played >= 10) m |= 1u << ACH_LIBRARY;
    if (best >= 3 * 3600) m |= 1u << ACH_MARATHON;
    if (top >= 25 * 3600) m |= 1u << ACH_DEVOTED;
    if (done >= 5) m |= 1u << ACH_FINISHER;
    if (favs >= 10) m |= 1u << ACH_CURATOR;
    if (ngenres >= 5) m |= 1u << ACH_EXPLORER;
    return m;
}

/* ---------------- views ---------------- */

static int sort_mode;
static int cmp_games(const void *a, const void *b) {
    const game_t *x = &g_games[*(const int *)a], *y = &g_games[*(const int *)b];
    switch (sort_mode) {
    case SORT_RECENT: if (x->last_played != y->last_played) return x->last_played > y->last_played ? -1 : 1; break;
    case SORT_PLAYTIME: if (x->playtime != y->playtime) return x->playtime > y->playtime ? -1 : 1; break;
    case SORT_YEAR: if (x->year != y->year) return x->year < y->year ? -1 : 1; break;
    case SORT_GENRE: { int c = strcasecmp(x->genre, y->genre); if (c) return c; break; }
    }
    return strcasecmp(x->name, y->name);
}

int library_view(int *out, int filter, int sort) {
    int n = 0;
    for (int i = 0; i < g_ngames; i++) {
        const game_t *g = &g_games[i];
        if ((filter == FILTER_APPS) != g->is_app && filter != FILTER_FAV && filter != FILTER_RECENT) continue;
        if (filter == FILTER_FAV && !g->favorite) continue;
        if (filter == FILTER_RECENT && !g->last_played) continue;
        if (filter == FILTER_PLAYING && !(g->collections & COL_PLAYING)) continue;
        if (filter == FILTER_BACKLOG && !(g->collections & COL_BACKLOG)) continue;
        if (filter == FILTER_DONE && !(g->collections & COL_DONE)) continue;
        if (filter == FILTER_MULTI && atoi(g->players) < 2) continue;
        out[n++] = i;
    }
    sort_mode = filter == FILTER_RECENT ? SORT_RECENT : sort;
    qsort(out, n, sizeof *out, cmp_games);
    return n;
}
