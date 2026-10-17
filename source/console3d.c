/* The Nintendo GameCube (Indigo) for the launch scene, modelled from photographs of the console
 * (150 x 110 x 161 mm). Parts, front to back:
 *   front   light-grey panel, four round controller ports with their 1-4 dots, two memory card doors
 *   top     rim, the disc lid with notches round the corner buttons, the round black badge with the
 *           lettering, POWER and RESET (Indigo, front corners), OPEN (light grey, back right), the
 *           embossed oval at the back left, the power light
 *   inside  bay walls, black turntable platform, chrome spindle hub, laser sled slot and lens,
 *           the warning label under the lid
 *   sides   8 x 8 vent grilles with their lip, the base seam, the feet
 *   back    the black carrying handle hanging in a U, the AV and power sockets
 * Units are millimetres; x right, y up, z towards the front. */
#include "scene3d.h"
#include "lid_badge_bin.h"
#include <math.h>

#define INDIGO 0x5D56A8FF
#define GREY_PANEL 0xBDBEB8FF
#define GREY_BTN 0xD4D6D2FF
#define BLACK 0x1A1A1EFF
#define GROOVE 0x2B2756FF

#define BW 75.0f                   /* half width */
#define BD 80.5f                   /* half depth */
#define BASE 4.0f                  /* feet height */
#define TOP 110.0f
#define RIM 5.5f
#define LID_HW 67.5f               /* lid half extents */
#define LID_Z0 (-BD + RIM + 1.0f)  /* back edge (hinge) */
#define LID_Z1 (BD - RIM - 1.0f)   /* front edge */
#define LID_T 3.5f
#define BTN_IN 9.5f                /* button centre, inset from the lid corner */
#define NOTCH 12.5f
#define BAY_Y 99.0f

static tex_t badge;
static bool loaded;

static inline vec3 V(float x, float y, float z) { return (vec3){x, y, z}; }
static uint32_t lit(uint32_t c, vec3 n, float gloss) { return shade_gloss(c, n, gloss, 26); }

/* ---------- small builders ---------- */

static void quadc(vec3 a, vec3 b, vec3 c, vec3 d, uint32_t col) { quad3(NULL, a, b, c, d, 0, 0, 0, 0, col, col, col, col); }

static void box(float x0, float y0, float z0, float x1, float y1, float z1, uint32_t col, float gloss) {
    quadc(V(x0, y1, z1), V(x1, y1, z1), V(x1, y0, z1), V(x0, y0, z1), lit(col, V(0, 0, 1), gloss));
    quadc(V(x1, y1, z0), V(x0, y1, z0), V(x0, y0, z0), V(x1, y0, z0), lit(col, V(0, 0, -1), gloss));
    quadc(V(x0, y1, z0), V(x0, y1, z1), V(x0, y0, z1), V(x0, y0, z0), lit(col, V(-1, 0, 0), gloss));
    quadc(V(x1, y1, z1), V(x1, y1, z0), V(x1, y0, z0), V(x1, y0, z1), lit(col, V(1, 0, 0), gloss));
    quadc(V(x0, y1, z0), V(x1, y1, z0), V(x1, y1, z1), V(x0, y1, z1), lit(col, V(0, 1, 0), gloss));
}

/* a flat round thing lying on a face: n = which face ('y' up, 'z' front, 'x' right / -x left) */
static void dot(float x, float y, float z, char face, float r, uint32_t col, int seg) {
    gfx_push();
    gfx_translate(x, y, z);
    if (face == 'y') gfx_rotate('x', -90);
    else if (face == 'x') gfx_rotate('y', 90);
    else if (face == 'X') gfx_rotate('y', -90);
    disc3(NULL, r, 0, col, seg);
    gfx_pop();
}

/* a short cylinder standing on the top: wall + cap, for buttons and the spindle */
static void puck(float cx, float cz, float r, float y0, float y1, uint32_t col, float gloss) {
    const int n = 28;
    for (int i = 0; i < n; i++) {
        float a0 = 2 * M_PI * i / n, a1 = 2 * M_PI * (i + 1) / n;
        vec3 n0 = V(cosf(a0), 0, sinf(a0)), n1 = V(cosf(a1), 0, sinf(a1));
        uint32_t c0 = lit(col, n0, gloss), c1 = lit(col, n1, gloss);
        quad3(NULL, V(cx + n0.x * r, y1, cz + n0.z * r), V(cx + n1.x * r, y1, cz + n1.z * r),
              V(cx + n1.x * r, y0, cz + n1.z * r), V(cx + n0.x * r, y0, cz + n0.z * r), 0, 0, 0, 0, c0, c1, c1, c0);
    }
    dot(cx, y1, cz, 'y', r, lit(col, V(0, 1, 0), gloss), n);
    dot(cx, y1 + 0.05f, cz, 'y', r * 0.72f, lit(col_scale(col, 1.04f), V(0, 1, 0), gloss * 1.4f), n);   /* domed centre */
}

/* ---------- outline of the lid: a rectangle with concave notches round the four corners ---------- */

#define LID_PTS 64
static float lx[LID_PTS], lz[LID_PTS];
static int lid_n;

static void lid_outline(void) {
    const float hw = LID_HW, z0 = LID_Z0, z1 = LID_Z1;
    /* corners in order: back-left, back-right, front-right, front-left (counter-clockwise from above) */
    const float cx[4] = {-hw, hw, hw, -hw}, cz[4] = {z0, z0, z1, z1};
    lid_n = 0;
    for (int c = 0; c < 4; c++) {
        float sx = cx[c] > 0 ? 1 : -1, sz = cz[c] > 0 ? 1 : -1;
        float bx = cx[c] - sx * (BTN_IN - 4.5f), bz = cz[c] - sz * (BTN_IN - 4.5f);    /* notch centre */
        float dz = sqrtf(NOTCH * NOTCH - (cx[c] - bx) * (cx[c] - bx));
        float dx = sqrtf(NOTCH * NOTCH - (cz[c] - bz) * (cz[c] - bz));
        /* the notch meets the side edge (x = cx) and the back / front edge (z = cz); walking round the
         * lid, corners 0 and 2 go side -> z edge, corners 1 and 3 z edge -> side */
        float psx = cx[c], psz = bz - sz * dz, pzx = bx - sx * dx, pzz = cz[c];
        float stx = c == 0 || c == 2 ? psx : pzx, stz = c == 0 || c == 2 ? psz : pzz;
        float enx = c == 0 || c == 2 ? pzx : psx, enz = c == 0 || c == 2 ? pzz : psz;
        float a0 = atan2f(stz - bz, stx - bx), a1 = atan2f(enz - bz, enx - bx);
        float da = a1 - a0;
        while (da > M_PI) da -= 2 * M_PI;
        while (da < -M_PI) da += 2 * M_PI;
        float mid = a0 + da / 2;   /* keep the arc that bites into the lid */
        if (fabsf(bx + cosf(mid) * NOTCH) > hw || fabsf(bz + sinf(mid) * NOTCH) > fabsf(cz[c]))
            da = da > 0 ? da - 2 * M_PI : da + 2 * M_PI;
        const int k = 12;
        for (int i = 0; i <= k; i++) {
            float a = a0 + da * i / k;
            lx[lid_n] = bx + cosf(a) * NOTCH;
            lz[lid_n] = bz + sinf(a) * NOTCH;
            lid_n++;
        }
    }
}

static float winding = 1;   /* +1 / -1 so that (-tz, tx) * winding points out of the lid */

static void lid_shape(float y0, float y1, uint32_t col, float gloss) {
    float zc = (LID_Z0 + LID_Z1) / 2;
    /* walls */
    for (int i = 0; i < lid_n; i++) {
        int j = (i + 1) % lid_n;
        float tx = lx[j] - lx[i], tz = lz[j] - lz[i], l = sqrtf(tx * tx + tz * tz);
        if (l < 1e-4f) continue;
        vec3 n = V(-tz / l * winding, 0, tx / l * winding);
        uint32_t c = lit(col, n, gloss);
        quadc(V(lx[i], y1, lz[i]), V(lx[j], y1, lz[j]), V(lx[j], y0, lz[j]), V(lx[i], y0, lz[i]), c);
    }
    /* top */
    vec3 ring[LID_PTS + 1];
    uint32_t cols[LID_PTS + 1];
    uint32_t ct = lit(col, V(0, 1, 0), gloss);
    for (int i = 0; i <= lid_n; i++) { ring[i] = V(lx[i % lid_n], y1, lz[i % lid_n]); cols[i] = ct; }
    fan3(V(0, y1, zc), ring, lid_n + 1, ct, cols);
}

/* ---------- parts ---------- */

static void shell(void) {
    /* feet */
    for (int i = 0; i < 4; i++) {
        float x = i & 1 ? BW - 14 : -BW + 4, z = i & 2 ? BD - 14 : -BD + 4;
        box(x, 0, z, x + 10, BASE, z + 10, BLACK, 0.2f);
    }
    /* body: a box with softened vertical edges (a 3 mm chamfer) */
    const float c = 3, y0 = BASE, y1 = TOP;
    float px[8] = {-BW + c, BW - c, BW, BW, BW - c, -BW + c, -BW, -BW};
    float pz[8] = {BD, BD, BD - c, -BD + c, -BD, -BD, -BD + c, BD - c};
    for (int i = 0; i < 8; i++) {
        int j = (i + 1) % 8;
        float nx = pz[j] - pz[i], nz = -(px[j] - px[i]), l = sqrtf(nx * nx + nz * nz);
        uint32_t col = lit(INDIGO, V(nx / l, 0, nz / l), 0.45f);
        quadc(V(px[i], y1 - 2, pz[i]), V(px[j], y1 - 2, pz[j]), V(px[j], y0, pz[j]), V(px[i], y0, pz[i]), col);
        /* rounded top edge: a 2 mm bevel up to the top face */
        float ix = px[i] * (BW - 2) / BW, iz = pz[i] * (BD - 2) / BD, jx = px[j] * (BW - 2) / BW, jz = pz[j] * (BD - 2) / BD;
        uint32_t cb = lit(INDIGO, V(nx / l, 1.2f, nz / l), 0.6f);
        quadc(V(ix, y1, iz), V(jx, y1, jz), V(px[j], y1 - 2, pz[j]), V(px[i], y1 - 2, pz[i]), cb);
    }
    /* base seam all the way round, 14 mm up */
    uint32_t g = GROOVE;
    quadc(V(-BW + 1, 18.6f, BD + 0.1f), V(BW - 1, 18.6f, BD + 0.1f), V(BW - 1, 17.8f, BD + 0.1f), V(-BW + 1, 17.8f, BD + 0.1f), g);
    quadc(V(BW + 0.1f, 18.6f, BD - 1), V(BW + 0.1f, 18.6f, -BD + 1), V(BW + 0.1f, 17.8f, -BD + 1), V(BW + 0.1f, 17.8f, BD - 1), g);
    quadc(V(-BW - 0.1f, 18.6f, -BD + 1), V(-BW - 0.1f, 18.6f, BD - 1), V(-BW - 0.1f, 17.8f, BD - 1), V(-BW - 0.1f, 17.8f, -BD + 1), g);
}

static void front(void) {
    const float z = BD + 0.12f;
    /* the light-grey panel with a darker seam round it */
    quadc(V(-70, 94, z), V(70, 94, z), V(70, 8, z), V(-70, 8, z), 0x3C3A52FF);
    quadc(V(-69.3f, 93.3f, z + 0.1f), V(69.3f, 93.3f, z + 0.1f), V(69.3f, 8.7f, z + 0.1f), V(-69.3f, 8.7f, z + 0.1f),
          lit(GREY_PANEL, V(0, 0, 1), 0.35f));
    /* four controller ports: a dark round socket with a rim, and 1-4 dots above */
    for (int p = 0; p < 4; p++) {
        float x = -47.5f + p * 31.7f;
        dot(x, 66, z + 0.2f, 'z', 7.6f, lit(0x9C9D97FF, V(0, 0, 1), 0.2f), 28);
        dot(x, 66, z + 0.3f, 'z', 6.4f, 0x141416FF, 28);
        dot(x, 64.5f, z + 0.4f, 'z', 2.6f, 0x2C2C30FF, 16);        /* the plug's key inside */
        for (int k = 0; k <= p; k++) dot(x + (k - p / 2.0f) * 2.6f, 78, z + 0.2f, 'z', 0.75f, 0x6E6F6AFF, 8);
    }
    /* memory card doors, slot A and slot B */
    for (int s = 0; s < 2; s++) {
        float x0 = s ? 4 : -64, x1 = s ? 64 : -4;
        quadc(V(x0, 44, z + 0.2f), V(x1, 44, z + 0.2f), V(x1, 22, z + 0.2f), V(x0, 22, z + 0.2f), 0x7E7F79FF);
        quadc(V(x0 + 0.8f, 43.2f, z + 0.3f), V(x1 - 0.8f, 43.2f, z + 0.3f), V(x1 - 0.8f, 22.8f, z + 0.3f), V(x0 + 0.8f, 22.8f, z + 0.3f),
              lit(col_scale(GREY_PANEL, 1.04f), V(0, 0, 1), 0.35f));
        quadc(V(x0 + 3, 40.5f, z + 0.4f), V(x1 - 3, 40.5f, z + 0.4f), V(x1 - 3, 39.6f, z + 0.4f), V(x0 + 3, 39.6f, z + 0.4f), 0x9A9B95FF);
        float m = (x0 + x1) / 2;   /* engraved label, two short rules */
        quadc(V(m - 7, 30, z + 0.4f), V(m + 7, 30, z + 0.4f), V(m + 7, 29.3f, z + 0.4f), V(m - 7, 29.3f, z + 0.4f), 0x8C8D87FF);
        quadc(V(m - 4, 27.6f, z + 0.4f), V(m + 4, 27.6f, z + 0.4f), V(m + 4, 26.9f, z + 0.4f), V(m - 4, 26.9f, z + 0.4f), 0x8C8D87FF);
    }
}

static void side(float sx) {
    /* the 8 x 8 vent grille toward the back, and its lip */
    const float x = sx * (BW + 0.12f), zc = -10, yc = 62, cell = 5.2f, pitch = 6.6f;
    uint32_t hole = 0x17142EFF, deep = 0x08070FFF;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            float z0 = zc + (c - 4) * pitch + (pitch - cell) / 2, y0 = yc + (r - 4) * pitch + (pitch - cell) / 2;
            float za = sx > 0 ? z0 + cell : z0, zb = sx > 0 ? z0 : z0 + cell;
            quadc(V(x, y0 + cell, za), V(x, y0 + cell, zb), V(x, y0, zb), V(x, y0, za), hole);
            /* the inner top wall of each hole catches no light: a darker strip */
            quadc(V(x + sx * 0.02f, y0 + cell, za), V(x + sx * 0.02f, y0 + cell, zb), V(x + sx * 0.02f, y0 + cell - 1.6f, zb),
                  V(x + sx * 0.02f, y0 + cell - 1.6f, za), deep);
        }
    /* the curved lip under the grille */
    for (int i = 0; i < 12; i++) {
        float t0 = i / 12.0f, t1 = (i + 1) / 12.0f;
        float za = zc - 30 + 60 * t0, zb = zc - 30 + 60 * t1;
        float ya = 25 + 6 * (1 - powf(2 * t0 - 1, 2)), yb = 25 + 6 * (1 - powf(2 * t1 - 1, 2));
        uint32_t c = lit(col_scale(INDIGO, 0.8f), V(sx, -0.4f, 0), 0.3f);
        quadc(V(x + sx * 0.05f, ya + 1.6f, za), V(x + sx * 0.05f, yb + 1.6f, zb), V(x + sx * 0.05f, yb, zb), V(x + sx * 0.05f, ya, za), c);
    }
    /* serial port cover outline near the bottom front */
    uint32_t g = GROOVE;
    float z0 = 26, z1 = 50, y0 = 5, y1 = 16;
    quadc(V(x, y1, z0), V(x, y1, z1), V(x, y1 - 0.7f, z1), V(x, y1 - 0.7f, z0), g);
    quadc(V(x, y1, z0), V(x, y1, z0 + 0.7f), V(x, y0, z0 + 0.7f), V(x, y0, z0), g);
    quadc(V(x, y1, z1 - 0.7f), V(x, y1, z1), V(x, y0, z1), V(x, y0, z1 - 0.7f), g);
}

static void back(void) {
    const float z = -BD - 0.12f;
    quadc(V(30, 40, z), V(10, 40, z), V(10, 28, z), V(30, 28, z), 0x0E0E12FF);    /* AV multi out */
    quadc(V(-12, 36, z), V(-24, 36, z), V(-24, 28, z), V(-12, 28, z), 0x0E0E12FF);  /* DC in */
    for (int i = 0; i < 7; i++) {                                                     /* vent slots */
        float x = -60 + i * 6;
        quadc(V(x + 3, 46, z), V(x, 46, z), V(x, 22, z), V(x + 3, 22, z), 0x24203FFF);
    }
    /* the carrying handle: a black band hinged near the top corners, hanging behind in a U */
    const float zb0 = -BD - 3, zb1 = -BD - 12, w = 12;
    const uint32_t hc = 0x18181CFF;
    float px[20], py[20];
    int n = 0;
    px[n] = -66; py[n++] = 96;
    px[n] = -66; py[n++] = 58;
    for (int i = 1; i <= 6; i++) { float a = M_PI + M_PI / 2 * i / 6; px[n] = -48 + cosf(a) * 18; py[n++] = 58 + sinf(a) * 18; }
    for (int i = 0; i <= 6; i++) { float a = -M_PI / 2 + M_PI / 2 * i / 6; px[n] = 48 + cosf(a) * 18; py[n++] = 58 + sinf(a) * 18; }
    px[n] = 66; py[n++] = 96;
    for (int i = 0; i < n - 1; i++) {
        float tx = px[i + 1] - px[i], ty = py[i + 1] - py[i], l = sqrtf(tx * tx + ty * ty);
        float nx = ty / l, ny = -tx / l;                     /* in-plane normal, outward of the U */
        float ax = px[i] + nx * w / 2, ay = py[i] + ny * w / 2, bx = px[i + 1] + nx * w / 2, by = py[i + 1] + ny * w / 2;
        float cx = px[i] - nx * w / 2, cy = py[i] - ny * w / 2, dx = px[i + 1] - nx * w / 2, dy = py[i + 1] - ny * w / 2;
        quadc(V(ax, ay, zb1), V(bx, by, zb1), V(dx, dy, zb1), V(cx, cy, zb1), lit(hc, V(0, 0, -1), 0.7f));   /* back face */
        quadc(V(ax, ay, zb0), V(bx, by, zb0), V(bx, by, zb1), V(ax, ay, zb1), lit(hc, V(nx, ny, 0), 0.7f));    /* outer edge */
        quadc(V(cx, cy, zb1), V(dx, dy, zb1), V(dx, dy, zb0), V(cx, cy, zb0), lit(hc, V(-nx, -ny, 0), 0.7f)); /* inner edge */
    }
    box(-70, 88, -BD - 4, -61, 100, -BD + 0.5f, hc, 0.5f);       /* hinge blocks */
    box(61, 88, -BD - 4, 70, 100, -BD + 0.5f, hc, 0.5f);
}

static void top(float led) {
    const float y = TOP;
    /* the top surface: a frame round the bay opening (the lid covers the middle) */
    uint32_t ct = lit(INDIGO, V(0, 1, 0), 0.5f);
    const float bx = LID_HW - 6, bz0 = LID_Z0 + 6, bz1 = LID_Z1 - 6;
    quadc(V(-BW + 2, y, -BD + 2), V(BW - 2, y, -BD + 2), V(BW - 2, y, bz0), V(-BW + 2, y, bz0), ct);
    quadc(V(-BW + 2, y, bz1), V(BW - 2, y, bz1), V(BW - 2, y, BD - 2), V(-BW + 2, y, BD - 2), ct);
    quadc(V(-BW + 2, y, bz0), V(-bx, y, bz0), V(-bx, y, bz1), V(-BW + 2, y, bz1), ct);
    quadc(V(bx, y, bz0), V(BW - 2, y, bz0), V(BW - 2, y, bz1), V(bx, y, bz1), ct);
    /* the bay: walls, floor, the black turntable platform, spindle, laser */
    uint32_t wall = col_scale(INDIGO, 0.85f);
    quadc(V(-bx, y, bz0), V(bx, y, bz0), V(bx, BAY_Y, bz0), V(-bx, BAY_Y, bz0), lit(wall, V(0, 0, 1), 0.2f));
    quadc(V(bx, y, bz1), V(-bx, y, bz1), V(-bx, BAY_Y, bz1), V(bx, BAY_Y, bz1), lit(wall, V(0, 0, -1), 0.2f));
    quadc(V(-bx, y, bz1), V(-bx, y, bz0), V(-bx, BAY_Y, bz0), V(-bx, BAY_Y, bz1), lit(wall, V(1, 0, 0), 0.2f));
    quadc(V(bx, y, bz0), V(bx, y, bz1), V(bx, BAY_Y, bz1), V(bx, BAY_Y, bz0), lit(wall, V(-1, 0, 0), 0.2f));
    quadc(V(-bx, BAY_Y, bz0), V(bx, BAY_Y, bz0), V(bx, BAY_Y, bz1), V(-bx, BAY_Y, bz1), lit(col_scale(INDIGO, 0.7f), V(0, 1, 0), 0.1f));
    const float tz = (bz0 + bz1) / 2;
    dot(0, BAY_Y + 0.2f, tz, 'y', 52, lit(0x101013FF, V(0, 1, 0), 0.35f), 56);         /* turntable platform */
    dot(0, BAY_Y + 0.3f, tz, 'y', 50.5f, lit(0x17171BFF, V(0, 1, 0), 0.35f), 56);
    puck(0, tz, 11, BAY_Y + 0.3f, BAY_Y + 3.5f, 0x1C1C20FF, 0.4f);                     /* spindle base */
    puck(0, tz, 5.2f, BAY_Y + 3.5f, BAY_Y + 6.0f, 0xC9CACFFF, 1.0f);                    /* chrome hub */
    for (int i = 0; i < 3; i++) {                                                        /* centring balls */
        float a = i * 2.094f;
        dot(cosf(a) * 8.2f, BAY_Y + 3.6f, tz + sinf(a) * 8.2f, 'y', 1.1f, 0xB0B1B6FF, 10);
    }
    {   /* laser sled slot from the spindle toward the back right, lens on the sled */
        float yy = BAY_Y + 0.45f;
        gfx_push(); gfx_translate(0, yy, tz); gfx_rotate('y', -35);
        quadc(V(14, 0, -5), V(44, 0, -5), V(44, 0, 5), V(14, 0, 5), 0x050507FF);
        quadc(V(22, 0.1f, -3.5f), V(32, 0.1f, -3.5f), V(32, 0.1f, 3.5f), V(22, 0.1f, 3.5f), 0x3A3A40FF);
        dot(27, 0.25f, 0, 'y', 1.8f, 0x6878FFFF, 14);
        gfx_pop();
    }
    /* corner buttons: POWER (front left) and RESET (front right) in Indigo, OPEN (back right) light grey */
    const float bxc = LID_HW - BTN_IN + 4.5f, bzf = LID_Z1 - BTN_IN + 4.5f, bzb = LID_Z0 + BTN_IN - 4.5f;
    dot(-bxc, y + 0.05f, bzf, 'y', 9.8f, GROOVE, 28);
    dot(bxc, y + 0.05f, bzf, 'y', 9.8f, GROOVE, 28);
    dot(bxc, y + 0.05f, bzb, 'y', 9.8f, GROOVE, 28);
    puck(-bxc, bzf, 8.6f, y, y + 2.2f, col_scale(INDIGO, 1.04f), 0.7f);
    puck(bxc, bzf, 8.6f, y, y + 2.2f, col_scale(INDIGO, 1.04f), 0.7f);
    puck(bxc, bzb, 8.6f, y, y + 2.6f, GREY_BTN, 0.8f);
    /* the embossed oval at the back left */
    gfx_push(); gfx_translate(-bxc, y + 0.08f, bzb); gfx_rotate('x', -90); gfx_scale(1.6f, 0.7f, 1);
    ring3(7.2f, 8.2f, lit(col_scale(INDIGO, 1.15f), V(0, 1, 0), 0.4f), 28);
    gfx_pop();
    /* power light, between POWER and the front edge */
    float lx0 = -bxc + 12, lz0 = BD - 4;
    dot(lx0, y + 0.1f, lz0, 'y', 1.5f, led > 0 ? col_mix(0x1B3A1EFF, 0x74FF84FF, led) : 0x22302AFF, 12);
    if (led > 0) {
        gfx_depth(false);
        dot(lx0, y + 0.2f, lz0, 'y', 6.5f, col_alpha(0x74FF84FF, led * 0.3f), 18);
        gfx_depth(true);
    }
}

static void lid(float open) {
    float zc = (LID_Z0 + LID_Z1) / 2;
    gfx_push();
    gfx_translate(0, TOP, LID_Z0 + 2);
    gfx_rotate('x', -70 * open);
    gfx_translate(0, 0, -(LID_Z0 + 2));
    lid_shape(0.04f, LID_T, INDIGO, 0.65f);
    /* the round black badge with the lettering, reading from the front */
    gfx_push();
    gfx_translate(0, LID_T + 0.06f, zc + 2);
    gfx_rotate('x', -90);
    uint32_t bc = shade_gloss(0xFFFFFFFF, V(0, 1, 0), 0.5f, 40);
    disc3(&badge, 37, 0, bc, 64);
    gfx_pop();
    /* the seam round the badge's bezel */
    gfx_push(); gfx_translate(0, LID_T + 0.05f, zc + 2); gfx_rotate('x', -90);
    ring3(37, 38.4f, lit(col_scale(INDIGO, 0.7f), V(0, 1, 0), 0.3f), 64);
    gfx_pop();
    if (open > 0.02f) {   /* underside, with the warning label */
        vec3 ring[LID_PTS + 1]; uint32_t cols[LID_PTS + 1];
        uint32_t cu = lit(col_scale(INDIGO, 0.8f), V(0, -1, 0), 0.3f);
        for (int i = 0; i <= lid_n; i++) { ring[i] = V(lx[i % lid_n], 0.02f, lz[i % lid_n]); cols[i] = cu; }
        fan3(V(0, 0.02f, zc), ring, lid_n + 1, cu, cols);
        uint32_t lab = 0xD8D9DCFF, ink = 0x55565CFF;
        float x0 = -30, x1 = 30, z0 = LID_Z0 + 12, z1 = LID_Z0 + 34, yy = -0.02f;
        quadc(V(x0, yy, z1), V(x1, yy, z1), V(x1, yy, z0), V(x0, yy, z0), lab);
        for (int i = 0; i < 4; i++) {
            float zz = z0 + 4 + i * 4.5f, xr = i == 3 ? 6 : 26;
            quadc(V(-26, yy - 0.02f, zz + 1), V(xr, yy - 0.02f, zz + 1), V(xr, yy - 0.02f, zz), V(-26, yy - 0.02f, zz), ink);
        }
        gfx_push(); gfx_translate(0, -0.05f, zc + 2); gfx_rotate('x', 90);
        disc3(NULL, 12, 0, 0xA6A7AEFF, 24);                                  /* disc clamp */
        gfx_pop();
    }
    gfx_pop();
}

void console3d(uint32_t unused, const tex_t *disc, float open, float disc_y, float disc_angle, float led, float a) {
    (void)unused; (void)a;
    if (!loaded) {
        tex_from_gxt(&badge, lid_badge_bin, lid_badge_bin_size, false);
        lid_outline();
        float area = 0;
        for (int i = 0; i < lid_n; i++) { int j = (i + 1) % lid_n; area += lx[i] * lz[j] - lx[j] * lz[i]; }
        winding = area > 0 ? -1 : 1;
        loaded = true;
    }
    /* contact shadow */
    gfx_depth(false);
    uint32_t sh = 0x00000070;
    quadc(V(-BW - 6, 0.1f, -BD - 6), V(BW + 6, 0.1f, -BD - 6), V(BW + 6, 0.1f, BD + 6), V(-BW - 6, 0.1f, BD + 6), sh);
    gfx_depth(true);
    shell();
    front();
    side(1);
    side(-1);
    back();
    top(led);
    if (disc_y >= 0) {
        gfx_push();
        gfx_translate(0, BAY_Y + 6.2f + disc_y, (LID_Z0 + LID_Z1) / 2);
        gfx_rotate('x', -90);
        gfx_rotate('z', disc_angle * 57.2958f);
        disc3d(disc, 0, 1);
        gfx_pop();
    }
    lid(open);
}
