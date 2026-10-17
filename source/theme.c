#include "theme.h"

/* The launch colours of the console: Indigo (the default), Jet Black, Platinum, Spice orange,
 * the Japan-only Emerald Blue and the limited Starlight Gold. */
const theme_t g_themes[THEME_COUNT] = {
    {"Indigo",       0x4B3F96FF, 0x140F33FF, 0x3A2E80FF, 0x9C8CFFFF, 0x6A5ACDFF, 0xFFFFFFFF, 0xE4DEFFFF, 0xA79FD6FF, 0x0C08245A, 0x2A2160FF},
    {"Jet Black",    0x1E1E22FF, 0x050506FF, 0x24242AFF, 0xD0D0D8FF, 0x7A7A86FF, 0xFFFFFFFF, 0xE6E6EEFF, 0x9A9AA6FF, 0x00000066, 0x18181CFF},
    {"Platinum",     0xC9CDD4FF, 0x3D4350FF, 0x8E96A5FF, 0xFFFFFFFF, 0xD5DAE3FF, 0xFFFFFFFF, 0xF1F3F7FF, 0xCBD1DBFF, 0x1A1E2850, 0x5F6675FF},
    {"Spice",        0xE8732AFF, 0x2E1206FF, 0x9A4214FF, 0xFFB46BFF, 0xF28A3CFF, 0xFFFFFFFF, 0xFFE9D6FF, 0xE3B596FF, 0x1A08005A, 0x6A2C0CFF},
    {"Emerald Blue", 0x197F86FF, 0x04211FFF, 0x0F5D5FFF, 0x6FE6D8FF, 0x2BB3AAFF, 0xFFFFFFFF, 0xDDFBF7FF, 0x94CFC9FF, 0x0012105A, 0x0A3E3FFF},
    {"Starlight Gold", 0xC9A646FF, 0x241B05FF, 0x7A6020FF, 0xFFE08AFF, 0xD9B24CFF, 0xFFFFFFFF, 0xFFF4D6FF, 0xD9C795FF, 0x1408005A, 0x4E3D12FF},
};

const theme_t *th = &g_themes[0];

void theme_set(int i) {
    if (i < 0 || i >= THEME_COUNT) i = 0;
    th = &g_themes[i];
}
