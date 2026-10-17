#include "fs.h"
#include <fat.h>
#include <sdcard/gcsd.h>
#include <ogc/dvd.h>
#include <iso9660.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* mount names match the device prefixes Swiss uses in its paths (gcldr:/, sdb:/ ...) so a
 * path read here can be handed to Swiss unchanged. */
static const struct { const char *name, *label; DISC_INTERFACE *io; bool iso; } devs[] = {
    {"gcldr", "GC Loader", &__io_gcode, false},
    {"sdc", "SD2SP2", &__io_gcsd2, false},
    {"sdb", "SD Gecko (slot B)", &__io_gcsdb, false},
    {"sda", "SD Gecko (slot A)", &__io_gcsda, false},
    /* last resort: an ISO9660 disc (the emulator test disc carries a sample library) */
    {"dvd", "DVD", &__io_gcdvd, true},
};

static char root[16];
static const char *label = "-";

static bool has_games(const char *name) {
    char p[32];
    struct stat st;
    snprintf(p, sizeof p, "%s:/games", name);
    if (stat(p, &st) == 0 && S_ISDIR(st.st_mode)) return true;
    snprintf(p, sizeof p, "%s:/cubemenu", name);
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

bool fs_init(void) {
    char first[16] = "";
    const char *first_label = "-";
    for (unsigned i = 0; i < sizeof devs / sizeof *devs; i++) {
        if (devs[i].iso ? !ISO9660_Mount(devs[i].name, devs[i].io) : !fatMountSimple(devs[i].name, devs[i].io)) continue;
        if (!first[0]) { snprintf(first, sizeof first, "%s", devs[i].name); first_label = devs[i].label; }
        if (has_games(devs[i].name)) {
            snprintf(root, sizeof root, "%s", devs[i].name);
            label = devs[i].label;
            return true;
        }
    }
    if (first[0]) { snprintf(root, sizeof root, "%s", first); label = first_label; return true; }
    return false;
}

const char *fs_root(void) { return root; }

void fs_shutdown(void) {
    char m[20];
    snprintf(m, sizeof m, "%s:", root);
    if (!strcmp(root, "dvd")) ISO9660_Unmount(root);
    else if (root[0]) fatUnmount(m);
}
const char *fs_label(void) { return label; }

char *fs_path(char *buf, int n, const char *rel) {
    snprintf(buf, n, "%s:%s%s", root, rel[0] == '/' ? "" : "/", rel);
    return buf;
}

bool fs_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

void *fs_read_all(const char *path, int *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *b = malloc(n + 1);
    if (b && fread(b, 1, n, f) != (size_t)n) { free(b); b = NULL; }
    fclose(f);
    if (b) { b[n] = 0; if (len) *len = n; }
    return b;
}

bool fs_write_all(const char *path, const void *data, int len) {
    char tmp[256];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, len, f) == (size_t)len;
    ok = fclose(f) == 0 && ok;
    if (ok) { remove(path); ok = rename(tmp, path) == 0; }
    else remove(tmp);
    return ok;
}
