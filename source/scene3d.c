#include "scene3d.h"
#include "ui.h"
#include <math.h>

static inline vec3 V(float x, float y, float z) { return (vec3){x, y, z}; }

static uint32_t A(uint32_t c, float a) { return col_alpha(c, a * (c & 0xFF) / 255.0f); }

/* axis-aligned box, flat shaded */
void box3(float x0, float y0, float z0, float x1, float y1, float z1, uint32_t col, float a) {
    uint32_t c;
    c = A(shade(col, V(0, 0, 1)), a);  quad3(NULL, V(x0, y1, z1), V(x1, y1, z1), V(x1, y0, z1), V(x0, y0, z1), 0, 0, 0, 0, c, c, c, c);
    c = A(shade(col, V(0, 0, -1)), a); quad3(NULL, V(x1, y1, z0), V(x0, y1, z0), V(x0, y0, z0), V(x1, y0, z0), 0, 0, 0, 0, c, c, c, c);
    c = A(shade(col, V(-1, 0, 0)), a); quad3(NULL, V(x0, y1, z0), V(x0, y1, z1), V(x0, y0, z1), V(x0, y0, z0), 0, 0, 0, 0, c, c, c, c);
    c = A(shade(col, V(1, 0, 0)), a);  quad3(NULL, V(x1, y1, z1), V(x1, y1, z0), V(x1, y0, z0), V(x1, y0, z1), 0, 0, 0, 0, c, c, c, c);
    c = A(shade(col, V(0, 1, 0)), a);  quad3(NULL, V(x0, y1, z0), V(x1, y1, z0), V(x1, y1, z1), V(x0, y1, z1), 0, 0, 0, 0, c, c, c, c);
    c = A(shade(col, V(0, -1, 0)), a); quad3(NULL, V(x0, y0, z1), V(x1, y0, z1), V(x1, y0, z0), V(x0, y0, z0), 0, 0, 0, 0, c, c, c, c);
}

/* ---------------- case ---------------- */

#define U_BACK1 0.4746f
#define U_FRONT0 0.5254f

void disc3d(const tex_t *disc, float angle, float a) {
    uint32_t c = col_alpha(0xFFFFFFFF, a);
    if (disc && disc->data) disc3(disc, DISC_R, angle, c, 40);
    else {
        disc3(NULL, DISC_R, 0, col_alpha(0xD8D8E0FF, a), 40);
        gfx_push(); gfx_translate(0, 0, 0.1f);
        ring3(DISC_R * 0.36f, DISC_R * 0.98f, col_alpha(0xB8B4D8FF, a * 0.5f), 40);
        gfx_pop();
    }
    gfx_push(); gfx_translate(0, 0, 0.2f);
    disc3(NULL, DISC_R * 0.19f, 0, col_alpha(0x2A2A30FF, a), 24);    /* clear hub */
    gfx_translate(0, 0, 0.1f);
    disc3(NULL, DISC_R * 0.075f, 0, col_alpha(0x050508FF, a), 16);    /* hole */
    gfx_pop();
}

void case3d(const tex_t *cover, const tex_t *disc, float a, float disc_out, float disc_angle, bool reflect) {
    const float w = CASE_W / 2, h = CASE_H / 2, d = CASE_D / 2;
    const uint32_t PLASTIC = 0x16161AFF;
    bool tex = cover && cover->data;
    uint32_t ct, cb;   /* top / bottom vertex colours (reflection fades toward the far edge) */
    if (reflect) { ct = col_alpha(0xFFFFFFFF, 0); cb = col_alpha(0xFFFFFFFF, a * 0.32f); }
    else { ct = cb = col_alpha(0xFFFFFFFF, a); }

    if (!reflect && disc_out > 0.001f) {
        gfx_push();
        gfx_translate(w - 52 + disc_out * 62, 18, 0);
        gfx_rotate('z', disc_angle * 57.2958f);
        disc3d(disc, 0, a);
        gfx_pop();
    }

    uint32_t sf = shade(0xFFFFFFFF, V(0, 0, 1)), sb = shade(0xFFFFFFFF, V(0, 0, -1)), ss = shade(0xFFFFFFFF, V(-1, 0, 0));
    #define M(c, s) (col_scale((c), ((s) >> 24) / 255.0f))
    if (tex) {
        quad3(cover, V(-w, h, d), V(w, h, d), V(w, -h, d), V(-w, -h, d), U_FRONT0, 0, 1, 1, M(ct, sf), M(ct, sf), M(cb, sf), M(cb, sf));
        quad3(cover, V(w, h, -d), V(-w, h, -d), V(-w, -h, -d), V(w, -h, -d), 0, 0, U_BACK1, 1, M(ct, sb), M(ct, sb), M(cb, sb), M(cb, sb));
        quad3(cover, V(-w, h, -d), V(-w, h, d), V(-w, -h, d), V(-w, -h, -d), U_BACK1, 0, U_FRONT0, 1, M(ct, ss), M(ct, ss), M(cb, ss), M(cb, ss));
    } else {
        uint32_t f0 = A(shade(0x2B2B33FF, V(0, 0, 1)), a), f1 = A(shade(0x111116FF, V(0, 0, 1)), a);
        if (reflect) { f0 = col_alpha(f0, 0); f1 = col_alpha(f1, a * 0.3f); }
        quad3(NULL, V(-w, h, d), V(w, h, d), V(w, -h, d), V(-w, -h, d), 0, 0, 0, 0, f0, f0, f1, f1);
        quad3(NULL, V(w, h, -d), V(-w, h, -d), V(-w, -h, -d), V(w, -h, -d), 0, 0, 0, 0, f0, f0, f1, f1);
        uint32_t sp = reflect ? col_alpha(0x202028FF, a * 0.2f) : A(shade(0x202028FF, V(-1, 0, 0)), a);
        quad3(NULL, V(-w, h, -d), V(-w, h, d), V(-w, -h, d), V(-w, -h, -d), 0, 0, 0, 0, sp, sp, sp, sp);
        if (!reflect) {   /* emblem on the blank front */
            uint32_t ec = col_alpha(0xFFFFFFFF, a * 0.55f);
            quad3(&g_tex_emblem, V(-30, 30, d + 0.3f), V(30, 30, d + 0.3f), V(30, -30, d + 0.3f), V(-30, -30, d + 0.3f), 0, 0, 1, 1, ec, ec, ec, ec);
        }
    }
    /* open side, top and bottom: black plastic */
    uint32_t pr = reflect ? col_alpha(PLASTIC, a * 0.25f) : A(shade(PLASTIC, V(1, 0, 0)), a);
    quad3(NULL, V(w, h, d), V(w, h, -d), V(w, -h, -d), V(w, -h, d), 0, 0, 0, 0, pr, pr, pr, pr);
    uint32_t pt = reflect ? col_alpha(PLASTIC, a * 0.1f) : A(shade(0x24242AFF, V(0, 1, 0)), a);
    quad3(NULL, V(-w, h, -d), V(w, h, -d), V(w, h, d), V(-w, h, d), 0, 0, 0, 0, pt, pt, pt, pt);
    quad3(NULL, V(-w, -h, d), V(w, -h, d), V(w, -h, -d), V(-w, -h, -d), 0, 0, 0, 0, pt, pt, pt, pt);

    if (!reflect) {
        /* clear sleeve: a soft diagonal sheen over the front */
        gfx_depth(false);
        uint32_t s0 = col_alpha(0xFFFFFFFF, a * 0.16f), s1 = col_alpha(0xFFFFFFFF, 0);
        quad3(NULL, V(-w, h, d + 0.2f), V(w * 0.2f, h, d + 0.2f), V(-w * 0.4f, -h * 0.3f, d + 0.2f), V(-w, -h * 0.1f, d + 0.2f), 0, 0, 0, 0, s0, s1, s1, s0);
        uint32_t e0 = col_alpha(0xFFFFFFFF, a * 0.28f);
        quad3(NULL, V(-w, h, d + 0.25f), V(w, h, d + 0.25f), V(w, h - 1.6f, d + 0.25f), V(-w, h - 1.6f, d + 0.25f), 0, 0, 0, 0, e0, e0, s1, s1);
        gfx_depth(true);
    }
    #undef M
}

/* ---------------- console ---------------- */

#define CW 75.0f      /* half width */
#define CH 110.0f     /* height */
#define CD 80.5f      /* half depth */
#define HINGE_Z (-38.0f)
#define LID_Z1 (CD - 6.0f)
#define LID_X 64.0f

static void flat(vec3 a, vec3 b, vec3 c, vec3 d, uint32_t col, vec3 n, float al) {
    uint32_t s = A(shade(col, n), al);
    quad3(NULL, a, b, c, d, 0, 0, 0, 0, s, s, s, s);
}

void console3d(uint32_t body, const tex_t *disc, float lid, float disc_y, float disc_angle, float led, float a) {
    uint32_t dark = col_scale(body, 0.55f), darker = col_scale(body, 0.30f);
    uint32_t top_col = col_scale(body, 1.06f);
    const float e = 7;                 /* bevel on the vertical edges */

    /* bottom shadow on the floor */
    gfx_depth(false);
    uint32_t sh = col_alpha(0x000000FF, a * 0.45f), sh0 = col_alpha(0x000000FF, 0);
    quad3(NULL, V(-CW - 14, 0.1f, -CD - 14), V(CW + 14, 0.1f, -CD - 14), V(CW + 14, 0.1f, CD + 18), V(-CW - 14, 0.1f, CD + 18), 0, 0, 0, 0, sh0, sh0, sh, sh);
    quad3(NULL, V(-CW, 0.2f, -CD), V(CW, 0.2f, -CD), V(CW, 0.2f, CD), V(-CW, 0.2f, CD), 0, 0, 0, 0, sh, sh, sh, sh);
    gfx_depth(true);

    /* body: an octagonal prism (bevelled corners) */
    float px[8] = {-CW + e, CW - e, CW, CW, CW - e, -CW + e, -CW, -CW};
    float pz[8] = {CD, CD, CD - e, -CD + e, -CD, -CD, -CD + e, CD - e};
    for (int i = 0; i < 8; i++) {
        int j = (i + 1) % 8;
        float nx = pz[j] - pz[i], nz = -(px[j] - px[i]);
        float l = sqrtf(nx * nx + nz * nz);
        flat(V(px[i], CH, pz[i]), V(px[j], CH, pz[j]), V(px[j], 0, pz[j]), V(px[i], 0, pz[i]), i == 0 ? col_scale(body, 0.94f) : body, V(nx / l, 0, nz / l), a);
    }
    /* top: fan over the octagon, minus nothing (the lid covers the front part) */
    uint32_t tc = A(shade(top_col, V(0, 1, 0)), a);
    for (int i = 1; i < 7; i++)
        quad3(NULL, V(px[0], CH, pz[0]), V(px[i], CH, pz[i]), V(px[i + 1], CH, pz[i + 1]), V(px[i + 1], CH, pz[i + 1]), 0, 0, 0, 0, tc, tc, tc, tc);
    /* front details: two memory card slots and four controller ports */
    float fz = CD + 0.25f;
    for (int s = 0; s < 2; s++) {
        float x0 = s ? 8 : -60, x1 = s ? 60 : -8;
        flat(V(x0, 58, fz), V(x1, 58, fz), V(x1, 52, fz), V(x0, 52, fz), darker, V(0, 0, 1), a);
    }
    for (int p = 0; p < 4; p++) {
        float cx = -48 + p * 32;
        flat(V(cx - 10, 30, fz), V(cx + 10, 30, fz), V(cx + 10, 18, fz), V(cx - 10, 18, fz), darker, V(0, 0, 1), a);
        flat(V(cx - 6, 27, fz + 0.1f), V(cx + 6, 27, fz + 0.1f), V(cx + 6, 21, fz + 0.1f), V(cx - 6, 21, fz + 0.1f), col_scale(body, 0.15f), V(0, 0, 1), a);
    }

    /* handle at the back */
    box3(-58, 72, -CD - 22, -50, 92, -CD, dark, a);
    box3(50, 72, -CD - 22, 58, 92, -CD, dark, a);
    box3(-58, 84, -CD - 24, 58, 94, -CD - 16, dark, a);

    /* tray recess under the lid */
    float ty = CH + 0.15f;
    flat(V(-LID_X + 4, ty, HINGE_Z + 4), V(LID_X - 4, ty, HINGE_Z + 4), V(LID_X - 4, ty, LID_Z1 - 4), V(-LID_X + 4, ty, LID_Z1 - 4), col_scale(body, 0.22f), V(0, 1, 0), a);
    float tcz = (HINGE_Z + LID_Z1) / 2;
    gfx_push();
    gfx_translate(0, ty + 0.1f, tcz);
    gfx_rotate('x', -90);
    disc3(NULL, 46, 0, col_alpha(0x0C0C10FF, a), 40);
    gfx_translate(0, 0, 0.1f);
    gfx_push(); gfx_translate(22, -8, 0);
    quad3(NULL, V(-4, 14, 0), V(4, 14, 0), V(4, -14, 0), V(-4, -14, 0), 0, 0, 0, 0, col_alpha(0x3A3A48FF, a), col_alpha(0x3A3A48FF, a), col_alpha(0x3A3A48FF, a), col_alpha(0x3A3A48FF, a));
    gfx_pop();
    disc3(NULL, 6, 0, col_alpha(0xA8A8B4FF, a), 16);         /* spindle */
    gfx_pop();

    /* the disc, lying flat, label up */
    if (disc_y >= 0) {
        gfx_push();
        gfx_translate(0, ty + 2.5f + disc_y, tcz);
        gfx_rotate('x', -90);
        gfx_rotate('z', disc_angle * 57.2958f);
        disc3d(disc, 0, a);
        gfx_pop();
    }

    /* buttons on the top front: OPEN (left) and POWER / RESET (right) */
    float bz = LID_Z1 + 2.5f;
    gfx_push(); gfx_translate(-52, CH + 0.4f, bz); gfx_rotate('x', -90);
    disc3(NULL, 5.5f, 0, A(shade(col_scale(body, 0.8f), V(0, 1, 0)), a), 16); gfx_pop();
    gfx_push(); gfx_translate(36, CH + 0.4f, bz); gfx_rotate('x', -90);
    disc3(NULL, 5.5f, 0, A(shade(col_scale(body, 0.8f), V(0, 1, 0)), a), 16); gfx_pop();
    gfx_push(); gfx_translate(52, CH + 0.4f, bz); gfx_rotate('x', -90);
    disc3(NULL, 3.5f, 0, A(shade(col_scale(body, 0.8f), V(0, 1, 0)), a), 16); gfx_pop();
    /* power LED */
    gfx_push(); gfx_translate(64, CH + 0.5f, bz); gfx_rotate('x', -90);
    disc3(NULL, 1.8f, 0, col_alpha(led > 0 ? col_mix(0x204020FF, 0x5CFF6AFF, led) : 0x202820FF, a), 12);
    gfx_pop();
    if (led > 0) {
        gfx_depth(false);
        gfx_push(); gfx_translate(64, CH + 0.6f, bz); gfx_rotate('x', -90);
        disc3(NULL, 7, 0, col_alpha(0x5CFF6AFF, a * led * 0.25f), 16);
        gfx_pop();
        gfx_depth(true);
    }

    /* lid: hinged at its back edge, opens upwards */
    gfx_push();
    gfx_translate(0, CH, HINGE_Z);
    gfx_rotate('x', -78 * lid);
    float L = LID_Z1 - HINGE_Z, T = 4.0f;
    uint32_t lc = col_scale(body, 1.02f);
    box3(-LID_X, 0.2f, 0, LID_X, T, L, lc, a);
    /* underside is darker */
    flat(V(-LID_X + 3, 0.1f, L - 3), V(LID_X - 3, 0.1f, L - 3), V(LID_X - 3, 0.1f, 3), V(-LID_X + 3, 0.1f, 3), col_scale(body, 0.45f), V(0, -1, 0), a);
    /* seam lines and the embossed emblem on top */
    float y = T + 0.15f;
    uint32_t seam = A(col_scale(body, 0.6f), a);
    quad3(NULL, V(-LID_X, y, 1.2f), V(LID_X, y, 1.2f), V(LID_X, y, 0), V(-LID_X, y, 0), 0, 0, 0, 0, seam, seam, seam, seam);
    uint32_t emb = col_alpha(col_mix(shade(lc, V(0, 1, 0)), 0xFFFFFFFF, 0.25f), a * 0.85f);
    float es = 26, ez = L * 0.52f;
    quad3(&g_tex_emblem, V(-es, y, ez - es), V(es, y, ez - es), V(es, y, ez + es), V(-es, y, ez + es), 0, 0, 1, 1, emb, emb, emb, emb);
    gfx_pop();
}
