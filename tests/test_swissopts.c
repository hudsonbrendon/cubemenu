/* Host check for swissopts.c: XXH64 against the reference vectors, and a settings + cheats round trip.
 *   cc -Isource -o /tmp/t tests/test_swissopts.c source/swissopts.c && /tmp/t */
#include "swissopts.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char root[64];
char *fs_path(char *b, int n, const char *rel) { snprintf(b, n, "%s%s", root, rel); return b; }
bool fs_exists(const char *p) { struct stat s; return stat(p, &s) == 0; }
void *fs_read_all(const char *p, int *len) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc(n + 1); n = fread(b, 1, n, f); b[n] = 0; fclose(f);
    if (len) *len = n; return b;
}
bool fs_write_all(const char *p, const void *d, int n) { FILE *f = fopen(p, "wb"); if (!f) return false; fwrite(d, 1, n, f); fclose(f); return true; }
uint64_t so_xxh64(const uint8_t *p, size_t len);

static void put(const char *rel, const char *txt) { char p[128]; fs_write_all(fs_path(p, sizeof p, rel), txt, strlen(txt)); }

int main(void) {
    assert(so_xxh64((const uint8_t *)"", 0) == 0xEF46DB3751D8E999ULL);
    assert(so_xxh64((const uint8_t *)"abc", 3) == 0x44BC2CF5AD770999ULL);
    const char *s70 = "0123456789012345678901234567890123456789012345678901234567890123456789";
    assert(so_xxh64((const uint8_t *)s70, 70) == 0x4916A0F3F0E1C781ULL);

    snprintf(root, sizeof root, "/tmp/cubemenu-so-%d", (int)getpid());
    char p[128];
    mkdir(root, 0777);
    mkdir(fs_path(p, sizeof p, "/swiss"), 0777);
    mkdir(fs_path(p, sizeof p, "/swiss/cheats"), 0777);
    mkdir(fs_path(p, sizeof p, "/swiss/settings"), 0777);
    put("/swiss/cheats/GTST01.txt", "[GTST01] Test\r\n\r\nInfinite Health\r\n04000000 00000001\r\n04000004 00000002\r\n\r\nBroken\r\n0400XXXX 00000000\r\n\r\nMoon Jump\r\n04000008 00000003\r\n");
    swiss_game_t s;
    so_load(&s, "GTST01");
    assert(s.ncheats == 2 && !strcmp(s.cheat_name[0], "Infinite Health") && !strcmp(s.cheat_name[1], "Moon Jump"));
    s.cheat_on[1] = 1;
    s.value[SO_VIDEO] = 5;           /* 480p */
    assert(so_save(&s, "GTST01", "Test"));
    so_free(&s);
    so_load(&s, "GTST01");
    assert(s.cheat_on[0] == 0 && s.cheat_on[1] == 1 && !strcmp(so_value(SO_VIDEO, s.value[SO_VIDEO]), "480p"));
    assert(so_has_cheats_on("GTST01"));
    s.cheat_on[1] = 0;
    so_save(&s, "GTST01", "Test");
    assert(!so_has_cheats_on("GTST01"));     /* none on: no selection file, like Swiss */
    so_free(&s);
    puts("swissopts: ok");
    return 0;
}
