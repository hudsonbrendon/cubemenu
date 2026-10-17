#pragma once
#include "gfx.h"
#include <stdbool.h>
#include <stdint.h>

/* Read-only view of the saves on a memory card (slot 0 = A, 1 = B). */
typedef struct {
    char file[33];
    char title[33], sub[33];     /* the save's comment lines */
    char gamecode[7];            /* game + maker code, e.g. GALE01 */
    uint32_t blocks;
    uint32_t time;               /* seconds since 2000-01-01 */
    tex_t icon;                  /* first icon frame, 32x32 RGB5A3 (data NULL when the save has none) */
} save_t;

/* fills out[], returns the count, -1 when no card is in the slot, -2 when it cannot be mounted */
int memcard_scan(int slot, save_t *out, int max, int *free_blocks);
void memcard_free(save_t *s, int n);
