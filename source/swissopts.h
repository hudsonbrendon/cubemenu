#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Swiss's own per-game files on the card, read and written the way Swiss does (interop only, no
 * Swiss code): swiss/settings/game/<ID4>.ini and swiss/cheats/<ID6>.txt + .chtsel */

enum { SO_VIDEO, SO_WIDESCREEN, SO_NO_DITHER, SO_ANISO, SO_COUNT };

typedef struct {
    int value[SO_COUNT];          /* index into so_values(i) */
    int ncheats;
    char **cheat_name;
    uint8_t *cheat_on;
} swiss_game_t;

int so_nvalues(int opt);
const char *so_value(int opt, int v);      /* the value as Swiss writes it ("480p", "3D", "Yes") */

void so_load(swiss_game_t *s, const char *id6);
bool so_save(const swiss_game_t *s, const char *id6, const char *name);
void so_free(swiss_game_t *s);
bool so_has_cheats_on(const char *id6);   /* a .chtsel exists: Swiss should apply them */
