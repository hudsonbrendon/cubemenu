#pragma once
#include "gfx.h"
#include <stdbool.h>

/* Per-game textures, loaded by a background thread around the cursor and evicted far from it. */
enum { AS_CASE, AS_DISC, AS_SNAP, AS_TITLE, AS_COUNT };

void assets_init(void);
/* list = indices into g_games in display order, focus = position in that list */
void assets_focus(const int *list, int n, int focus, int radius);
/* NULL until loaded (or missing) */
const tex_t *assets_get(int game, int kind);
bool assets_missing(int game, int kind);
void assets_want_shots(int game);   /* details screen asks for screenshots */
void assets_stop(void);
