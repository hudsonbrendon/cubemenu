#include "boot.h"
#include "fs.h"
#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char boot_stub[], boot_stub_end[];

/* 0x80001800 is avoided on purpose: emulators treat a jump there as "exit to the loader" */
#define STUB_ADDR ((void *)0x80002000)
#define OPS_ADDR ((uint32_t *)0x80002800)
#define ARGV_ADDR ((struct __argv *)0x80002C00)   /* low memory: nothing the new DOL loads lands here */
#define MAX_OPS 24

typedef struct { uint32_t off[18], addr[18], size[18], bss, bss_size, entry; } dol_hdr;

static const char *SWISS_PATHS[] = {"/cubemenu/swiss.dol", "/swiss/boot.dol", "/swiss/swiss.dol", "/autoexec.dol", NULL};

const char *boot_find_swiss(void) {
    static char path[128];
    for (int i = 0; SWISS_PATHS[i]; i++) {
        fs_path(path, sizeof path, SWISS_PATHS[i]);
        if (fs_exists(path)) return path;
    }
    return NULL;
}

static bool overlaps(uint32_t a0, uint32_t a1, uint32_t b0, uint32_t b1) { return a0 < b1 && b0 < a1; }

bool boot_dol(const char *path, int nargs, const char **args, void (*before_jump)(void), char *err, int errlen) {
    FILE *f = fopen(path, "rb");
    if (!f) { snprintf(err, errlen, "open %s", path); return false; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < (long)sizeof(dol_hdr) || size > 16 << 20) { fclose(f); snprintf(err, errlen, "bad DOL size"); return false; }

    dol_hdr h;
    if (fread(&h, 1, sizeof h, f) != sizeof h) { fclose(f); snprintf(err, errlen, "read header"); return false; }
    /* every byte the new program will own: sections, bss, and the command line placed after them */
    uint32_t lo = 0xFFFFFFFF, hi = 0;
    for (int i = 0; i < 18; i++) {
        if (!h.size[i]) continue;
        if (h.off[i] + h.size[i] > (uint32_t)size || h.addr[i] < 0x80003000 || h.addr[i] + h.size[i] > 0x81800000) {
            fclose(f); snprintf(err, errlen, "bad DOL section %d", i); return false;
        }
        if (h.addr[i] < lo) lo = h.addr[i];
        if (h.addr[i] + h.size[i] > hi) hi = h.addr[i] + h.size[i];
    }
    if (h.bss_size && h.bss + h.bss_size > hi) hi = h.bss + h.bss_size;
    if (h.bss_size && h.bss < lo) lo = h.bss;

    /* command line: argv[0] = path, then args, NUL separated */
    int clen = strlen(path) + 1;
    for (int i = 0; i < nargs; i++) clen += strlen(args[i]) + 1;
    char *cmd = malloc(clen);
    char *p = cmd;
    p += sprintf(p, "%s", path) + 1;
    for (int i = 0; i < nargs; i++) p += sprintf(p, "%s", args[i]) + 1;
    uint32_t cmd_addr = (hi + 31) & ~31u;
    uint32_t top = cmd_addr + clen + 256;       /* room for the argv pointer array crt0 builds */

    /* the file buffer must not sit where the new program lands; park blocks until it doesn't */
    void *park[8];
    int nparked = 0;
    uint8_t *buf = NULL;
    for (;;) {
        buf = memalign(32, size);
        if (!buf) break;
        if (!overlaps((uint32_t)buf, (uint32_t)buf + size, lo, top) &&
            !overlaps((uint32_t)cmd, (uint32_t)cmd + clen, lo, top)) break;
        if (nparked == 8) { free(buf); buf = NULL; break; }
        park[nparked++] = buf;
        buf = NULL;
    }
    if (!buf) {
        while (nparked) free(park[--nparked]);
        free(cmd); fclose(f); snprintf(err, errlen, "no memory for the DOL"); return false;
    }
    memcpy(buf, &h, sizeof h);
    bool ok = fread(buf + sizeof h, 1, size - sizeof h, f) == (size_t)(size - sizeof h);
    fclose(f);
    if (!ok) { free(buf); free(cmd); snprintf(err, errlen, "read DOL"); return false; }

    /* argv struct goes right after the entry branch when the program advertises it */
    bool wants_argv = false;
    for (int i = 0; i < 18; i++) {
        if (h.size[i] && h.entry >= h.addr[i] && h.entry + 8 <= h.addr[i] + h.size[i]) {
            uint32_t magic = *(uint32_t *)(buf + h.off[i] + (h.entry - h.addr[i]) + 4);
            wants_argv = magic == ARGV_MAGIC;
        }
    }

    if (before_jump) before_jump();
    VIDEO_SetBlack(true);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
    IRQ_Disable();

    /* nothing else runs from here on: lay out the stub, its op list and argv in low memory */
    struct __argv *av = ARGV_ADDR;
    memset(av, 0, sizeof *av);
    av->argvMagic = ARGV_MAGIC;
    av->commandLine = (char *)cmd_addr;
    av->length = clen;
    uint32_t *ops = OPS_ADDR;
    int n = 0;
    uint32_t *o = ops + 2;
    for (int i = 0; i < 18 && n < MAX_OPS - 3; i++) {
        if (!h.size[i]) continue;
        o[0] = h.addr[i]; o[1] = (uint32_t)(buf + h.off[i]); o[2] = h.size[i]; o += 3; n++;
    }
    if (h.bss_size) { o[0] = h.bss; o[1] = 0; o[2] = h.bss_size; o += 3; n++; }
    if (wants_argv) {
        o[0] = cmd_addr; o[1] = (uint32_t)cmd; o[2] = clen; o += 3; n++;
        o[0] = h.entry + 8; o[1] = (uint32_t)av; o[2] = sizeof *av; o += 3; n++;
    }
    ops[0] = n;
    ops[1] = h.entry;
    int stub_len = boot_stub_end - boot_stub;
    memcpy(STUB_ADDR, boot_stub, stub_len);
    DCFlushRange(buf, size);
    DCFlushRange(cmd, clen);
    DCFlushRange((void *)0x80002000, 0x1000);
    ICInvalidateRange(STUB_ADDR, stub_len);
    ((void (*)(uint32_t *))STUB_ADDR)(ops);
    return false;   /* not reached */
}
