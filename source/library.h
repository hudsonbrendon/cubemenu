#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define MAX_GAMES 512
#define MAX_DISCS 4
#define NLANG 3

typedef struct {
    char id[7];
    char name[64];
    char folder[128];                 /* "/games/F-Zero GX" */
    char disc[MAX_DISCS][40];         /* file names, disc order */
    int ndisc;
    /* meta (info.txt from the card tool) */
    char developer[48], publisher[48], genre[40], players[8], rating[16];
    int year;
    char *synopsis[NLANG];            /* pt, en, es; NULL when missing */
    bool has_clip, has_music, has_shot[2], has_manual;
    uint32_t hltb[3];                 /* HowLongToBeat: main story, + extras, completionist (seconds, 0 = unknown) */
    /* stats */
    uint32_t playtime;                /* seconds */
    uint32_t launches;
    uint32_t last_played;             /* unix time */
    bool favorite;
    uint8_t collections;              /* COL_* bits */
    uint32_t best_session;            /* longest credited session, seconds */
    /* homebrew from /apps/<folder>: folder is "/apps/<folder>", exe[] its .dol / .dol+cli files */
    bool is_app;
    char (*exe)[48];
    int nexe;
} game_t;

enum { COL_PLAYING = 1, COL_BACKLOG = 2, COL_DONE = 4 };

extern game_t g_games[MAX_GAMES];
extern int g_ngames;

void library_scan(void);
const char *game_synopsis(const game_t *g, int lang);     /* falls back to another language */
const char *game_genre(const game_t *g, int lang);        /* translated when known */
void game_asset_path(char *buf, int n, const game_t *g, const char *file);   /* /cubemenu/games/<ID>/file */
int game_launch_path(char *buf, int n, const game_t *g, int disc);            /* "gcldr:/games/X/game.iso" */
/* "gcldr:/apps/gbi/gbi-ossc.dol+cli" and its arguments (the .cli next to a .dol, or the text after
 * the DOL in a .dol+cli); returns the argument count, args point into *store (free() it) */
int app_launch(const game_t *g, int exe, char *path, int n, const char **args, int maxargs, char **store);
const char *app_variant(const game_t *g, int exe);    /* "ossc" for gbi-ossc.dol+cli, NULL for the main one */
/* achievements: bit mask of the unlocked ones (ACH_COUNT of them) */
enum { ACH_FIRST, ACH_10H, ACH_50H, ACH_100H, ACH_LIBRARY, ACH_MARATHON, ACH_DEVOTED, ACH_FINISHER, ACH_CURATOR, ACH_EXPLORER, ACH_COUNT };
uint32_t achievements(void);
uint32_t achievements_new(void);      /* unlocked since the last boot (read by stats_load) */

/* stats (play time is measured across reboots: the launch time is saved, the next boot of the
 * menu credits the elapsed time to that game) */
void stats_load(void);
void stats_save(void);
void stats_mark_launch(game_t *g);
const char *stats_credit_message(void);
bool stats_just_played(void);             /* a game was started less than 6 h before this boot: back from it */   /* "+1h 20m in Pikmin" after a credited session, else NULL */
void fmt_playtime(char *buf, int n, uint32_t secs);

/* views over the library */
enum { FILTER_ALL, FILTER_FAV, FILTER_RECENT, FILTER_PLAYING, FILTER_BACKLOG, FILTER_DONE, FILTER_MULTI, FILTER_APPS, FILTER_COUNT };
enum { SORT_NAME, SORT_RECENT, SORT_PLAYTIME, SORT_YEAR, SORT_GENRE, SORT_COUNT };
int library_view(int *out, int filter, int sort);
