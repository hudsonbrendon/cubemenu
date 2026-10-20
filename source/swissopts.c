#include "swissopts.h"
#include "fs.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>

/* ---------------- per-game settings ---------------- */

static const char *const KEYS[SO_COUNT] = {"Force Video Mode", "Force Widescreen", "Disable Alpha Dithering", "Force Anisotropic Filter"};
static const char *const VIDEO[] = {"Auto", "480i", "480sf", "240p", "960i", "480p", "1080i60", "540p60", "576i", "576sf", "288p", "1152i", "576p", "1080i50", "540p50"};
static const char *const WIDE[] = {"No", "3D", "2D+3D"};
static const char *const YESNO[] = {"No", "Yes"};

int so_nvalues(int opt) {
    return opt == SO_VIDEO ? (int)(sizeof VIDEO / sizeof *VIDEO) : opt == SO_WIDESCREEN ? 3 : 2;
}

const char *so_value(int opt, int v) {
    if (v < 0 || v >= so_nvalues(opt)) v = 0;
    return opt == SO_VIDEO ? VIDEO[v] : opt == SO_WIDESCREEN ? WIDE[v] : YESNO[v];
}

static void game_ini_path(char *buf, int n, const char *id6) {
    char rel[64];
    snprintf(rel, sizeof rel, "/swiss/settings/game/%.4s.ini", id6);
    fs_path(buf, n, rel);
}

static void cheats_path(char *buf, int n, const char *id6, const char *ext) {
    char rel[64];
    snprintf(rel, sizeof rel, "/swiss/cheats/%.6s.%s", id6, ext);
    fs_path(buf, n, rel);
}

/* "Key=Value" -> value of one of our keys, -1 when the line is not ours */
static int our_key(const char *line, const char **val) {
    for (int i = 0; i < SO_COUNT; i++) {
        size_t n = strlen(KEYS[i]);
        if (!strncmp(line, KEYS[i], n) && line[n] == '=') { *val = line + n + 1; return i; }
    }
    return -1;
}

/* ---------------- cheats: the same reading of the .txt as Swiss, so the selection lines up ---------------- */

static bool ishex(int c) { return isxdigit(c); }

/* scanf-like: 1..max chars of class at s; returns how many */
static int span(const char *s, int max, bool hex) {
    int k = 0;
    while (k < max && s[k] && (hex ? ishex((unsigned char)s[k]) : isalnum((unsigned char)s[k]))) k++;
    return k;
}

static int ws(const char *s) { int k = 0; while (s[k] && isspace((unsigned char)s[k])) k++; return k; }

/* "%*2[hex]%*6[alnum] %*8[alnum]" consuming more than 16 characters */
static bool is_cheat_code(const char *l) {
    int a = span(l, 2, true); if (!a) return false;
    int b = span(l + a, 6, false); if (!b) return false;
    int c = ws(l + a + b);
    int d = span(l + a + b + c, 8, false); if (!d) return false;
    return a + b + c + d > 16;
}

/* "%*8[hex] %*8[hex]" consuming more than 16 characters */
static bool is_valid_code(const char *l) {
    int a = span(l, 8, true); if (!a) return false;
    int c = ws(l + a);
    int d = span(l + a + c, 8, true); if (!d) return false;
    return a + c + d > 16;
}

/* XXH64, seed 0 (the hash Swiss keeps next to a selection) */
#define P1 0x9E3779B185EBCA87ULL
#define P2 0xC2B2AE3D27D4EB4FULL
#define P3 0x165667B19E3779F9ULL
#define P4 0x85EBCA77C2B2AE63ULL
#define P5 0x27D4EB2F165667C5ULL
static uint64_t rotl(uint64_t x, int r) { return x << r | x >> (64 - r); }
static uint64_t le64(const uint8_t *p) { uint64_t v = 0; for (int i = 7; i >= 0; i--) v = v << 8 | p[i]; return v; }
static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint64_t xround(uint64_t acc, uint64_t in) { acc += in * P2; return rotl(acc, 31) * P1; }
static uint64_t xmerge(uint64_t acc, uint64_t v) { acc ^= xround(0, v); return acc * P1 + P4; }

uint64_t so_xxh64(const uint8_t *p, size_t len) {
    const uint8_t *end = p + len;
    uint64_t h;
    if (len >= 32) {
        uint64_t v1 = P1 + P2, v2 = P2, v3 = 0, v4 = -P1;
        do {
            v1 = xround(v1, le64(p)); v2 = xround(v2, le64(p + 8));
            v3 = xround(v3, le64(p + 16)); v4 = xround(v4, le64(p + 24));
            p += 32;
        } while (p + 32 <= end);
        h = rotl(v1, 1) + rotl(v2, 7) + rotl(v3, 12) + rotl(v4, 18);
        h = xmerge(h, v1); h = xmerge(h, v2); h = xmerge(h, v3); h = xmerge(h, v4);
    } else h = P5;
    h += len;
    for (; p + 8 <= end; p += 8) { h ^= xround(0, le64(p)); h = rotl(h, 27) * P1 + P4; }
    if (p + 4 <= end) { h ^= (uint64_t)le32(p) * P1; h = rotl(h, 23) * P2 + P3; p += 4; }
    for (; p < end; p++) { h ^= *p * P5; h = rotl(h, 11) * P1; }
    h ^= h >> 33; h *= P2; h ^= h >> 29; h *= P3; h ^= h >> 32;
    return h;
}

/* parses the cheats, fills names (when s is given) and the hash input (the codes, big endian) */
static int parse_cheats(char *txt, swiss_game_t *s, uint8_t **codes, size_t *ncodes) {
    char *lines[2048];
    int n = 0;
    for (char *l = strtok(txt, "\r\n"); l && n < 2048; l = strtok(NULL, "\r\n")) lines[n++] = l;
    int count = 0;
    size_t cap = 4096, len = 0;
    uint8_t *buf = malloc(cap);
    const char *prev = NULL;
    for (int i = 0; i < n;) {
        if (!is_cheat_code(lines[i])) { prev = lines[i++]; continue; }
        bool unsupported = false;
        size_t start = len;
        int j = i;
        for (; j < n && is_cheat_code(lines[j]); j++) {
            if (!is_valid_code(lines[j])) { unsupported = true; continue; }
            unsigned a = 0, b = 0;
            sscanf(lines[j], "%x %x", &a, &b);
            if (len + 8 > cap) buf = realloc(buf, cap *= 2);
            uint8_t *q = buf + len;
            q[0] = a >> 24; q[1] = a >> 16; q[2] = a >> 8; q[3] = a; q[4] = b >> 24; q[5] = b >> 16; q[6] = b >> 8; q[7] = b;
            len += 8;
        }
        if (unsupported) len = start;
        else {
            if (s) {
                s->cheat_name = realloc(s->cheat_name, (count + 1) * sizeof *s->cheat_name);
                char def[24];
                snprintf(def, sizeof def, "Cheat %d", count + 1);
                s->cheat_name[count] = strdup(prev ? prev : def);
            }
            count++;
        }
        prev = j < n ? lines[j] : NULL;
        i = j + 1;
    }
    *codes = buf;
    *ncodes = len;
    return count;
}

void so_load(swiss_game_t *s, const char *id6) {
    memset(s, 0, sizeof *s);
    char path[128];
    game_ini_path(path, sizeof path, id6);
    char *txt = fs_read_all(path, NULL);
    if (txt) {
        for (char *l = strtok(txt, "\r\n"); l; l = strtok(NULL, "\r\n")) {
            const char *v;
            int k = our_key(l, &v);
            if (k < 0) continue;
            for (int i = 0; i < so_nvalues(k); i++) if (!strcasecmp(v, so_value(k, i))) s->value[k] = i;
        }
        free(txt);
    }
    cheats_path(path, sizeof path, id6, "txt");
    txt = fs_read_all(path, NULL);
    if (!txt) return;
    uint8_t *codes;
    size_t ncodes;
    s->ncheats = parse_cheats(txt, s, &codes, &ncodes);
    free(txt);
    s->cheat_on = calloc(s->ncheats + 1, 1);
    /* the selection only counts when the hash still matches the codes */
    cheats_path(path, sizeof path, id6, "chtsel");
    int len = 0;
    uint8_t *sel = fs_read_all(path, &len);
    if (sel && len == s->ncheats + 8) {
        uint64_t h;
        memcpy(&h, sel + s->ncheats, 8);         /* native order, as Swiss writes it */
        if (h == so_xxh64(codes, ncodes)) memcpy(s->cheat_on, sel, s->ncheats);
    }
    free(sel);
    free(codes);
}

bool so_save(const swiss_game_t *s, const char *id6, const char *name) {
    char path[128];
    bool ok = true;
    /* settings: keep every line that is not ours, then ours when not the default */
    game_ini_path(path, sizeof path, id6);
    char *old = fs_read_all(path, NULL);
    bool any = false;
    for (int i = 0; i < SO_COUNT; i++) any |= s->value[i] != 0;
    if (old || any) {
        int cap = (old ? strlen(old) : 0) + 512;
        char *out = malloc(cap), *p = out;
        if (old) {
            for (char *l = strtok(old, "\r\n"); l; l = strtok(NULL, "\r\n")) {
                const char *v;
                if (our_key(l, &v) < 0) p += sprintf(p, "%s\r\n", l);
            }
        } else p += sprintf(p, "# Game specific configuration file. Created by Swiss\r\nID=%.4s\r\nName=%.64s\r\n", id6, name);
        for (int i = 0; i < SO_COUNT; i++) if (s->value[i]) p += sprintf(p, "%s=%s\r\n", KEYS[i], so_value(i, s->value[i]));
        mkdir(fs_path(path, sizeof path, "/swiss/settings/game"), 0777);
        game_ini_path(path, sizeof path, id6);
        ok = fs_write_all(path, out, p - out);
        free(out);
        free(old);
    }
    /* cheats: one byte per cheat + the hash, or no file when none is on (like Swiss) */
    if (s->ncheats) {
        cheats_path(path, sizeof path, id6, "txt");
        char *txt = fs_read_all(path, NULL);
        if (!txt) return ok;
        uint8_t *codes;
        size_t ncodes;
        int n = parse_cheats(txt, NULL, &codes, &ncodes);
        free(txt);
        bool on = false;
        for (int i = 0; i < n && i < s->ncheats; i++) on |= s->cheat_on[i];
        cheats_path(path, sizeof path, id6, "chtsel");
        if (!on || n != s->ncheats) unlink(path);
        else {
            uint8_t *sel = malloc(n + 8);
            memcpy(sel, s->cheat_on, n);
            uint64_t h = so_xxh64(codes, ncodes);
            memcpy(sel + n, &h, 8);
            ok &= fs_write_all(path, sel, n + 8);
            free(sel);
        }
        free(codes);
    }
    return ok;
}

void so_free(swiss_game_t *s) {
    for (int i = 0; i < s->ncheats; i++) free(s->cheat_name[i]);
    free(s->cheat_name);
    free(s->cheat_on);
    memset(s, 0, sizeof *s);
}

bool so_has_cheats_on(const char *id6) {
    char path[128];
    cheats_path(path, sizeof path, id6, "chtsel");
    return fs_exists(path);
}
