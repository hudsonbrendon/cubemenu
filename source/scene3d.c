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
