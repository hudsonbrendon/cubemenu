#pragma once
#include "gfx.h"
#include <stdbool.h>

/* Background clip player: a thread streams fixed-size CMPR frames ("CMV1") into a ring of textures. */
void clip_init(void);
void clip_play(const char *path);   /* NULL stops */
const tex_t *clip_frame(void);      /* NULL until the first frame is in */
float clip_age(void);               /* seconds since the first frame was shown */
void clip_stop(void);
