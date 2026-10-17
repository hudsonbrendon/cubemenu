#include "gfx.h"
#include <malloc.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <ogc/lwp_watchdog.h>

#define FIFO_SIZE (256 * 1024)
#define MSTACK 16

static GXRModeObj *rmode;
static void *xfb[2];
static int fb;
static Mtx view, model, mstack[MSTACK];
static int msp;
static int textured = -1;
static const GXTexObj *bound;
static volatile bool tex_dirty = true;

uint32_t col_mix(uint32_t a, uint32_t b, float t) {
    uint32_t r = 0;
    for (int s = 0; s < 32; s += 8) {
        float va = (a >> s) & 0xFF, vb = (b >> s) & 0xFF;
        r |= (uint32_t)clampf(va + (vb - va) * t, 0, 255) << s;
    }
    return r;
}

uint32_t col_scale(uint32_t c, float k) {
    uint32_t r = c & 0xFF;
    for (int s = 8; s < 32; s += 8) r |= (uint32_t)clampf(((c >> s) & 0xFF) * k, 0, 255) << s;
    return r;
}

GXRModeObj *gfx_mode(void) { return rmode; }

void gfx_init(void) {
    VIDEO_Init();
    rmode = VIDEO_GetPreferredMode(NULL);
    for (int i = 0; i < 2; i++) xfb[i] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    VIDEO_Configure(rmode);
    VIDEO_ClearFrameBuffer(rmode, xfb[0], COLOR_BLACK);
    VIDEO_ClearFrameBuffer(rmode, xfb[1], COLOR_BLACK);
    VIDEO_SetNextFramebuffer(xfb[0]);
    VIDEO_SetBlack(false);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

    void *fifo = memalign(32, FIFO_SIZE);
    memset(fifo, 0, FIFO_SIZE);
    GX_Init(fifo, FIFO_SIZE);
    GXColor black = {0, 0, 0, 255};
    GX_SetCopyClear(black, GX_MAX_Z24);
    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
    f32 yscale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
    u32 xfbh = GX_SetDispCopyYScale(yscale);
    GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopyDst(rmode->fbWidth, xfbh);
    GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
    GX_SetFieldMode(rmode->field_rendering, (rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE);
    GX_SetPixelFmt(rmode->aa ? GX_PF_RGB565_Z16 : GX_PF_RGB8_Z24, GX_ZC_LINEAR);
    GX_SetDispCopyGamma(GX_GM_1_0);
    GX_SetCullMode(GX_CULL_NONE);

    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GX_SetNumChans(1);
    GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
    GX_SetNumTexGens(1);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GX_SetNumTevStages(1);
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
    GX_SetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_ALWAYS, 0);
    GX_SetZCompLoc(GX_FALSE);
    GX_SetAlphaUpdate(GX_TRUE);
    GX_SetColorUpdate(GX_TRUE);
    guMtxIdentity(model);
}

static void set_textured(const tex_t *t) {
    if (t) {
        if (textured != 1) {
            GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
            GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
            textured = 1;
        }
        if (bound != &t->obj) { GX_LoadTexObj((GXTexObj *)&t->obj, GX_TEXMAP0); bound = &t->obj; }
    } else if (textured != 0) {
        GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
        GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
        textured = 0;
    }
}

void gfx_begin(uint32_t clear) {
    if (tex_dirty) { tex_dirty = false; GX_InvalidateTexAll(); }
    GXColor c = {clear >> 24, clear >> 16, clear >> 8, 255};
    GX_SetCopyClear(c, GX_MAX_Z24);
    bound = NULL;
    textured = -1;
    gfx_2d();
}

float g_ms_gpu, g_ms_vsync;   /* last frame: waiting for GX_DrawDone, waiting for vsync */

void gfx_end(void) {
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GX_SetColorUpdate(GX_TRUE);
    GX_CopyDisp(xfb[fb], GX_TRUE);
    u64 t0 = gettime();
    GX_DrawDone();
    u64 t1 = gettime();
    VIDEO_SetNextFramebuffer(xfb[fb]);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    u64 t2 = gettime();
    g_ms_gpu = ticks_to_microsecs(diff_ticks(t0, t1)) / 1000.0f;
    g_ms_vsync = ticks_to_microsecs(diff_ticks(t1, t2)) / 1000.0f;
    fb ^= 1;
}

void gfx_fade_black(void) {
    VIDEO_SetBlack(true);
    VIDEO_Flush();
    VIDEO_WaitVSync();
}

/* ---------------- textures ---------------- */

static int tex_bytes(int w, int h, int fmt) {
    switch (fmt) {
    case GX_TF_CMPR: return w * h / 2;
    case GX_TF_I8: return w * h;
    case GX_TF_RGBA8: return w * h * 4;
    default: return w * h * 2;
    }
}

static void tex_init_obj(tex_t *t) {
    GX_InitTexObj(&t->obj, t->data, t->w, t->h, t->fmt, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjLOD(&t->obj, GX_LINEAR, GX_LINEAR, 0, 0, 0, GX_FALSE, GX_FALSE, GX_ANISO_1);
}

bool tex_from_gxt(tex_t *t, const void *buf, int len, bool copy) {
    const uint8_t *b = buf;
    memset(t, 0, sizeof *t);
    if (len < 32 || memcmp(b, "GXT1", 4)) return false;
    t->w = b[4] << 8 | b[5];
    t->h = b[6] << 8 | b[7];
    t->fmt = b[8];
    uint32_t size = b[12] << 24 | b[13] << 16 | b[14] << 8 | b[15];
    if (size + 32 > (uint32_t)len) return false;
    if (copy || ((uintptr_t)(b + 32) & 31)) {
        t->data = memalign(32, size);
        if (!t->data) return false;
        memcpy(t->data, b + 32, size);
    } else {
        t->data = (void *)(b + 32);
        t->borrowed = 1;
    }
    DCFlushRange(t->data, size);
    tex_init_obj(t);
    return true;
}

bool tex_load(tex_t *t, const char *path) {
    memset(t, 0, sizeof *t);
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    uint8_t h[32];
    bool ok = false;
    if (fread(h, 1, 32, f) == 32 && !memcmp(h, "GXT1", 4)) {
        t->w = h[4] << 8 | h[5]; t->h = h[6] << 8 | h[7]; t->fmt = h[8];
        uint32_t size = h[12] << 24 | h[13] << 16 | h[14] << 8 | h[15];
        if (size == (uint32_t)tex_bytes(t->w, t->h, t->fmt) && size < 4 << 20 && (t->data = memalign(32, size))) {
            ok = fread(t->data, 1, size, f) == size;
            if (ok) { DCFlushRange(t->data, size); tex_init_obj(t); gfx_invalidate(); }
            else { free(t->data); t->data = NULL; }
        }
    }
    fclose(f);
    return ok;
}

bool tex_create(tex_t *t, int w, int h, int fmt) {
    memset(t, 0, sizeof *t);
    t->w = w; t->h = h; t->fmt = fmt;
    t->data = memalign(32, tex_bytes(w, h, fmt));
    if (!t->data) return false;
    memset(t->data, 0, tex_bytes(w, h, fmt));
    tex_update(t);
    return true;
}

void tex_update(tex_t *t) {
    DCFlushRange(t->data, tex_bytes(t->w, t->h, t->fmt));
    tex_init_obj(t);
    gfx_invalidate();
}

/* texture memory caches by address: needed whenever bytes under an address already drawn change */
void gfx_invalidate(void) { tex_dirty = true; }

void tex_free(tex_t *t) {
    if (t->data && !t->borrowed) free(t->data);
    memset(t, 0, sizeof *t);
}

/* ---------------- 2D ---------------- */

void gfx_2d(void) {
    Mtx44 p;
    Mtx mv;
    guOrtho(p, 0, SCREEN_H, 0, SCREEN_W, 0, 300);
    GX_LoadProjectionMtx(p, GX_ORTHOGRAPHIC);
    guMtxIdentity(mv);
    guMtxTransApply(mv, mv, 0, 0, -100);
    GX_LoadPosMtxImm(mv, GX_PNMTX0);
    GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
}

static inline void vtx(float x, float y, float z, uint32_t c, float u, float v) {
    GX_Position3f32(x, y, z);
    GX_Color1u32(c);
    GX_TexCoord2f32(u, v);
}

void draw_rect_grad(float x, float y, float w, float h, uint32_t top, uint32_t bottom) {
    set_textured(NULL);
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    vtx(x, y, 0, top, 0, 0); vtx(x + w, y, 0, top, 0, 0);
    vtx(x + w, y + h, 0, bottom, 0, 0); vtx(x, y + h, 0, bottom, 0, 0);
    GX_End();
}

void draw_rect_grad_h(float x, float y, float w, float h, uint32_t left, uint32_t right) {
    set_textured(NULL);
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    vtx(x, y, 0, left, 0, 0); vtx(x + w, y, 0, right, 0, 0);
    vtx(x + w, y + h, 0, right, 0, 0); vtx(x, y + h, 0, left, 0, 0);
    GX_End();
}

void draw_rect(float x, float y, float w, float h, uint32_t c) { draw_rect_grad(x, y, w, h, c, c); }

static void fan_arc(float cx, float cy, float r, float a0, uint32_t c) {
    const int n = 6;
    GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, n + 2);
    vtx(cx, cy, 0, c, 0, 0);
    for (int i = 0; i <= n; i++) {
        float a = a0 + (M_PI / 2) * i / n;
        vtx(cx + cosf(a) * r, cy + sinf(a) * r, 0, c, 0, 0);
    }
    GX_End();
}

void draw_round_rect(float x, float y, float w, float h, float r, uint32_t c) {
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    if (r < 1) { draw_rect(x, y, w, h, c); return; }
    set_textured(NULL);
    draw_rect(x + r, y, w - 2 * r, h, c);
    draw_rect(x, y + r, r, h - 2 * r, c);
    draw_rect(x + w - r, y + r, r, h - 2 * r, c);
    fan_arc(x + r, y + r, r, M_PI, c);
    fan_arc(x + w - r, y + r, r, -M_PI / 2, c);
    fan_arc(x + w - r, y + h - r, r, 0, c);
    fan_arc(x + r, y + h - r, r, M_PI / 2, c);
}

void draw_tex_uv(const tex_t *t, float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t c) {
    if (!t || !t->data) return;
    set_textured(t);
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    vtx(x, y, 0, c, u0, v0); vtx(x + w, y, 0, c, u1, v0);
    vtx(x + w, y + h, 0, c, u1, v1); vtx(x, y + h, 0, c, u0, v1);
    GX_End();
}

void draw_tex(const tex_t *t, float x, float y, float w, float h, uint32_t c) { draw_tex_uv(t, x, y, w, h, 0, 0, 1, 1, c); }

void draw_tex_quad(const tex_t *t, const float *p, float u0, float v0, float u1, float v1, const uint32_t *c) {
    if (!t || !t->data) return;
    set_textured(t);
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    vtx(p[0], p[1], 0, c[0], u0, v0); vtx(p[2], p[3], 0, c[1], u1, v0);
    vtx(p[4], p[5], 0, c[2], u1, v1); vtx(p[6], p[7], 0, c[3], u0, v1);
    GX_End();
}

void draw_quad_flat(const float *p, const uint32_t *c) {
    set_textured(NULL);
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    vtx(p[0], p[1], 0, c[0], 0, 0); vtx(p[2], p[3], 0, c[1], 0, 0);
    vtx(p[4], p[5], 0, c[2], 0, 0); vtx(p[6], p[7], 0, c[3], 0, 0);
    GX_End();
}

void draw_circle(float cx, float cy, float r, uint32_t c) {
    int n = r > 60 ? 48 : r > 16 ? 28 : 16;
    set_textured(NULL);
    GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, n + 2);
    vtx(cx, cy, 0, c, 0, 0);
    for (int i = 0; i <= n; i++) {
        float a = 2 * M_PI * i / n;
        vtx(cx + cosf(a) * r, cy + sinf(a) * r, 0, c, 0, 0);
    }
    GX_End();
}

void draw_ring(float cx, float cy, float r0, float r1, uint32_t c) {
    int n = r1 > 60 ? 48 : 28;
    set_textured(NULL);
    GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT0, 2 * (n + 1));
    for (int i = 0; i <= n; i++) {
        float a = 2 * M_PI * i / n, ca = cosf(a), sa = sinf(a);
        vtx(cx + ca * r0, cy + sa * r0, 0, c, 0, 0);
        vtx(cx + ca * r1, cy + sa * r1, 0, c, 0, 0);
    }
    GX_End();
}

void draw_glow(float cx, float cy, float r, uint32_t c) {
    const int n = 32;
    set_textured(NULL);
    GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, n + 2);
    vtx(cx, cy, 0, c, 0, 0);
    for (int i = 0; i <= n; i++) {
        float a = 2 * M_PI * i / n;
        vtx(cx + cosf(a) * r, cy + sinf(a) * r, 0, c & 0xFFFFFF00, 0, 0);
    }
    GX_End();
}

void draw_tex_disc(const tex_t *t, float cx, float cy, float r, float angle, uint32_t c) {
    if (!t || !t->data) return;
    const int n = 40;
    set_textured(t);
    GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, n + 2);
    vtx(cx, cy, 0, c, 0.5f, 0.5f);
    for (int i = 0; i <= n; i++) {
        float a = 2 * M_PI * i / n, ca = cosf(a), sa = sinf(a);
        float ta = a + angle;
        vtx(cx + ca * r, cy + sa * r, 0, c, 0.5f + cosf(ta) * 0.5f, 0.5f + sinf(ta) * 0.5f);
    }
    GX_End();
}

/* ---------------- 3D ---------------- */

static guVector light = {-0.35f, 0.75f, 0.56f};

static vec3 view_dir = {0, 0.5f, 0.87f};   /* towards the eye, world space */

void gfx_3d(float fovy, vec3 eye, vec3 at) { gfx_3d_shift(fovy, eye, at, 0); }

void gfx_3d_shift(float fovy, vec3 eye, vec3 at, float shift_px) {
    Mtx44 p;
    guPerspective(p, fovy, (float)SCREEN_W / SCREEN_H, 10.0f, 5000.0f);
    /* move the picture up by shift_px without changing the view: offset in NDC after projection */
    p[1][2] -= 2.0f * shift_px / SCREEN_H;
    GX_LoadProjectionMtx(p, GX_PERSPECTIVE);
    float vx = eye.x - at.x, vy = eye.y - at.y, vz = eye.z - at.z, vl = sqrtf(vx * vx + vy * vy + vz * vz);
    if (vl > 0) view_dir = (vec3){vx / vl, vy / vl, vz / vl};
    guVector e = {eye.x, eye.y, eye.z}, a = {at.x, at.y, at.z}, up = {0, 1, 0};
    guLookAt(view, &e, &up, &a);
    guMtxIdentity(model);
    msp = 0;
    gfx_load_model();
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
}

void gfx_depth(bool on) { GX_SetZMode(on ? GX_TRUE : GX_FALSE, GX_LEQUAL, on ? GX_TRUE : GX_FALSE); }

void gfx_push(void) { if (msp < MSTACK) guMtxCopy(model, mstack[msp++]); }
void gfx_pop(void) { if (msp > 0) { guMtxCopy(mstack[--msp], model); gfx_load_model(); } }

void gfx_translate(float x, float y, float z) {
    Mtx t;
    guMtxTrans(t, x, y, z);
    guMtxConcat(model, t, model);
    gfx_load_model();
}

void gfx_rotate(char axis, float deg) {
    Mtx r;
    guMtxRotDeg(r, axis, deg);
    guMtxConcat(model, r, model);
    gfx_load_model();
}

void gfx_scale(float x, float y, float z) {
    Mtx s;
    guMtxScale(s, x, y, z);
    guMtxConcat(model, s, model);
    gfx_load_model();
}

void gfx_load_model(void) {
    Mtx mv;
    guMtxConcat(view, model, mv);
    GX_LoadPosMtxImm(mv, GX_PNMTX0);
}

uint32_t shade(uint32_t base, vec3 n) {
    /* model-space normal -> world (rotation part of the model matrix) */
    float wx = model[0][0] * n.x + model[0][1] * n.y + model[0][2] * n.z;
    float wy = model[1][0] * n.x + model[1][1] * n.y + model[1][2] * n.z;
    float wz = model[2][0] * n.x + model[2][1] * n.y + model[2][2] * n.z;
    float len = sqrtf(wx * wx + wy * wy + wz * wz);
    if (len < 1e-5f) return base;
    float d = (wx * light.x + wy * light.y + wz * light.z) / len;
    return col_scale(base, 0.42f + 0.62f * clampf(d, 0, 1));
}

/* plastic: diffuse key light + ambient + a Blinn highlight towards the eye */
uint32_t shade_gloss(uint32_t base, vec3 n, float gloss, float power) {
    float wx = model[0][0] * n.x + model[0][1] * n.y + model[0][2] * n.z;
    float wy = model[1][0] * n.x + model[1][1] * n.y + model[1][2] * n.z;
    float wz = model[2][0] * n.x + model[2][1] * n.y + model[2][2] * n.z;
    float len = sqrtf(wx * wx + wy * wy + wz * wz);
    if (len < 1e-5f) return base;
    wx /= len; wy /= len; wz /= len;
    float d = wx * light.x + wy * light.y + wz * light.z;
    float k = 0.38f + 0.70f * clampf(d, 0, 1);
    float hx = light.x + view_dir.x, hy = light.y + view_dir.y, hz = light.z + view_dir.z;
    float hl = sqrtf(hx * hx + hy * hy + hz * hz);
    float sp = clampf((wx * hx + wy * hy + wz * hz) / hl, 0, 1);
    sp = powf(sp, power) * gloss;
    return col_mix(col_scale(base, k), (base & 0xFF) | 0xFFFFFF00, clampf(sp, 0, 1));
}

void fan3(vec3 c, const vec3 *ring, int n, uint32_t cc, const uint32_t *cols) {
    set_textured(NULL);
    GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, n + 1);
    vtx(c.x, c.y, c.z, cc, 0, 0);
    for (int i = 0; i < n; i++) vtx(ring[i].x, ring[i].y, ring[i].z, cols[i], 0, 0);
    GX_End();
}

void quad3(const tex_t *t, vec3 a, vec3 b, vec3 c, vec3 d, float u0, float v0, float u1, float v1,
           uint32_t ca, uint32_t cb, uint32_t cc, uint32_t cd) {
    set_textured(t && t->data ? t : NULL);
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    vtx(a.x, a.y, a.z, ca, u0, v0); vtx(b.x, b.y, b.z, cb, u1, v0);
    vtx(c.x, c.y, c.z, cc, u1, v1); vtx(d.x, d.y, d.z, cd, u0, v1);
    GX_End();
}

void disc3(const tex_t *t, float r, float angle, uint32_t c, int seg) {
    set_textured(t && t->data ? t : NULL);
    GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, seg + 2);
    vtx(0, 0, 0, c, 0.5f, 0.5f);
    for (int i = 0; i <= seg; i++) {
        float a = 2 * M_PI * i / seg, ta = a + angle;
        vtx(cosf(a) * r, sinf(a) * r, 0, c, 0.5f + cosf(ta) * 0.5f, 0.5f - sinf(ta) * 0.5f);
    }
    GX_End();
}

void ring3(float r0, float r1, uint32_t c, int seg) {
    set_textured(NULL);
    GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT0, 2 * (seg + 1));
    for (int i = 0; i <= seg; i++) {
        float a = 2 * M_PI * i / seg, ca = cosf(a), sa = sinf(a);
        vtx(ca * r0, sa * r0, 0, c, 0, 0);
        vtx(ca * r1, sa * r1, 0, c, 0, 0);
    }
    GX_End();
}
