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
    bool has_clip, has_music, has_shot[2];
    /* stats */
    uint32_t playtime;                /* seconds */
    uint32_t launches;
    uint32_t last_played;             /* unix time */
    bool favorite;
} game_t;

extern game_t g_games[MAX_GAMES];
extern int g_ngames;

void library_scan(void);
const char *game_synopsis(const game_t *g, int lang);     /* falls back to another language */
void game_asset_path(char *buf, int n, const game_t *g, const char *file);   /* /cubemenu/games/<ID>/file */
int game_launch_path(char *buf, int n, const game_t *g, int disc);            /* "gcldr:/games/X/game.iso" */

/* stats (play time is measured across reboots: the launch time is saved, the next boot of the
 * menu credits the elapsed time to that game) */
void stats_load(void);
void stats_save(void);
void stats_mark_launch(game_t *g);
const char *stats_credit_message(void);   /* "+1h 20m in Pikmin" after a credited session, else NULL */
void fmt_playtime(char *buf, int n, uint32_t secs);

/* views over the library */
enum { FILTER_ALL, FILTER_FAV, FILTER_RECENT, FILTER_COUNT };
enum { SORT_NAME, SORT_RECENT, SORT_PLAYTIME, SORT_YEAR, SORT_COUNT };
int library_view(int *out, int filter, int sort);
