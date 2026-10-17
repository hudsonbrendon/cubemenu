#pragma once
#include "gfx.h"

/* GameCube keep case, millimetres: 135 x 191 x 14, spine on the left. Model origin = case centre. */
#define CASE_W 135.0f
#define CASE_H 191.0f
#define CASE_D 14.0f
#define DISC_R 40.0f          /* 8 cm mini disc */

void box3(float x0, float y0, float z0, float x1, float y1, float z1, uint32_t col, float alpha);
/* tex: full cover (back|spine|front) or NULL for a blank case; disc_out 0..1 slides the disc out
 * of the right side; reflect draws the mirrored floor copy (alpha fades away from the floor) */
void case3d(const tex_t *cover, const tex_t *disc, float alpha, float disc_out, float disc_angle, bool reflect);
/* the disc on its own, in the XY plane (label towards +Z) */
void disc3d(const tex_t *disc, float angle, float alpha);

/* GameCube console for the launch scene. Origin: floor centre; 150 x 110 x 161 mm.
 * lid: 0 closed .. 1 open; disc_y: height of the disc above the tray (<0 hides it); led 0..1 */
void console3d(uint32_t body, const tex_t *disc, float lid, float disc_y, float disc_angle, float led, float alpha);
