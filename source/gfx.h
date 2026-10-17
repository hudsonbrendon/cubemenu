#pragma once
#include <gccore.h>
#include <stdbool.h>
#include <stdint.h>

#define SCREEN_W 640
#define SCREEN_H 480

typedef struct {
    GXTexObj obj;
    void *data;          /* 32-byte aligned, owned unless 'borrowed' */
    uint16_t w, h;
    uint8_t fmt, borrowed;
} tex_t;

typedef struct { float x, y, z; } vec3;

/* colours are 0xRRGGBBAA */
#define RGBA(r, g, b, a) (((uint32_t)(r) << 24) | ((uint32_t)(g) << 16) | ((uint32_t)(b) << 8) | (uint32_t)(a))
#define HEXA(rgb, a) (((uint32_t)(rgb) << 8) | ((uint32_t)(a) & 0xFF))
static inline uint32_t col_alpha(uint32_t c, float a) {
    float v = (c & 0xFF) * a; if (v < 0) v = 0; if (v > 255) v = 255;
    return (c & 0xFFFFFF00) | (uint32_t)v;
}
uint32_t col_mix(uint32_t a, uint32_t b, float t);
uint32_t col_scale(uint32_t c, float k);   /* rgb * k, alpha kept */

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline float ease_out(float t) { t = clampf(t, 0, 1); return 1 - (1 - t) * (1 - t) * (1 - t); }
static inline float ease_io(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

void gfx_init(void);
void gfx_begin(uint32_t clear);
void gfx_end(void);
GXRModeObj *gfx_mode(void);
void gfx_fade_black(void);

/* textures */
bool tex_from_gxt(tex_t *t, const void *buf, int len, bool copy);   /* .gxt in memory */
bool tex_load(tex_t *t, const char *path);                            /* .gxt file */
bool tex_create(tex_t *t, int w, int h, int fmt);                     /* blank, for streaming */
void tex_free(tex_t *t);
void tex_update(tex_t *t);                                            /* after writing t->data */
void gfx_invalidate(void);                                            /* texture bytes changed */

/* 2D (screen pixels) */
void gfx_2d(void);
void draw_rect(float x, float y, float w, float h, uint32_t c);
void draw_rect_grad(float x, float y, float w, float h, uint32_t top, uint32_t bottom);
void draw_rect_grad_h(float x, float y, float w, float h, uint32_t left, uint32_t right);
void draw_round_rect(float x, float y, float w, float h, float r, uint32_t c);
void draw_tex(const tex_t *t, float x, float y, float w, float h, uint32_t c);
void draw_tex_uv(const tex_t *t, float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t c);
void draw_tex_quad(const tex_t *t, const float *xy /* 4 pts: tl tr br bl */, float u0, float v0, float u1, float v1, const uint32_t *cols);
void draw_quad_flat(const float *xy, const uint32_t *cols);      /* untextured, tl tr br bl */
void draw_circle(float cx, float cy, float r, uint32_t c);
void draw_ring(float cx, float cy, float r0, float r1, uint32_t c);
void draw_glow(float cx, float cy, float r, uint32_t c);                 /* soft radial */
void draw_tex_disc(const tex_t *t, float cx, float cy, float r, float angle, uint32_t c);

/* 3D */
void gfx_3d(float fovy, vec3 eye, vec3 at);
void gfx_push(void);          /* model matrix stack */
void gfx_pop(void);
void gfx_translate(float x, float y, float z);
void gfx_rotate(char axis, float deg);
void gfx_scale(float x, float y, float z);
void gfx_load_model(void);
void gfx_depth(bool on);
/* quad with explicit corners (model space), tl tr br bl */
void quad3(const tex_t *t, vec3 a, vec3 b, vec3 c, vec3 d, float u0, float v0, float u1, float v1, uint32_t ca, uint32_t cb, uint32_t cc, uint32_t cd);
void disc3(const tex_t *t, float r, float angle, uint32_t c, int seg);       /* textured disc in XY plane, z=0 */
void ring3(float r0, float r1, uint32_t c, int seg);
/* shade a base colour by a face normal against the fixed key light */
uint32_t shade(uint32_t base, vec3 n);
