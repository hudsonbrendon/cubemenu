/* Cube Menu apploader. The IPL copies this to 0x81200000 and asks it, through main(), which disc
 * ranges to read; close() returns the DOL entry point. The DOL offset is stored at 0x420 of the
 * disc header (boot.bin), sections are read straight to their load addresses. */
typedef unsigned int u32;
typedef void (*report_t)(const char *, ...);

/* everything lives in .data so it is part of the image the IPL copies (nothing past it is ours) */
static u32 hdr[8] __attribute__((aligned(32), section(".data"))) = {1};
static u32 dol[64] __attribute__((aligned(32), section(".data"))) = {1};
static int state __attribute__((section(".data"))) = -1, sec __attribute__((section(".data"))) = -1;

static void dc_invalidate(void *p, u32 n) {
    for (u32 a = (u32)p & ~31u; a < (u32)p + n; a += 32) __asm__ volatile("dcbi 0,%0" ::"r"(a));
}
/* The drive writes memory by DMA, behind the data cache. Destinations are invalidated (never written
 * back: a dirty line left by the IPL would overwrite what the drive just put there) and, at the end,
 * dropped from the instruction cache. */
static void ic_invalidate(u32 p, u32 n) {
    for (u32 a = p & ~31u; a < p + n; a += 32) __asm__ volatile("icbi 0,%0" ::"r"(a));
    __asm__ volatile("sync; isync");
}

static void ap_init(report_t r) { (void)r; state = 0; sec = 0; }

static int ap_main(void **dst, u32 *size, u32 *off) {
    switch (state) {
    case 0:                                   /* boot.bin 0x420: dol offset */
        dc_invalidate(hdr, sizeof hdr);
        *dst = hdr; *size = 0x20; *off = 0x420; state = 1;
        return 1;
    case 1:
        dc_invalidate(hdr, sizeof hdr);      /* the drive filled it: read the fresh bytes */
        dc_invalidate(dol, sizeof dol);
        *dst = dol; *size = 0x100; *off = hdr[0]; state = 2;
        return 1;
    case 2:
        if (sec == 0) dc_invalidate(dol, sizeof dol);
        while (sec < 18) {
            int i = sec++;
            if (!dol[36 + i] || !dol[18 + i]) continue;
            *dst = (void *)dol[18 + i];
            *size = (dol[36 + i] + 31) & ~31u;
            dc_invalidate(*dst, *size);
            *off = hdr[0] + dol[i];
            return 1;
        }
        state = 3;
        return 0;
    }
    return 0;
}

static void *ap_close(void) {
    for (int i = 0; i < 18; i++)
        if (dol[36 + i]) { dc_invalidate((void *)dol[18 + i], dol[36 + i]); ic_invalidate(dol[18 + i], dol[36 + i]); }
    /* BSS is cleared by the DOL's own startup code */
    return (void *)dol[56];
}

void ap_entry(void (**init)(report_t), int (**mainp)(void **, u32 *, u32 *), void *(**closep)(void)) {
    *init = ap_init;
    *mainp = ap_main;
    *closep = ap_close;
}
