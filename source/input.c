#include "input.h"
#include <gccore.h>

static uint32_t held, prev, pressed, injected;
static int repeat_timer, idle;

void input_init(void) { PAD_Init(); }

static uint32_t read_pad(void) {
    PAD_ScanPads();
    uint32_t b = 0;
    for (int i = 0; i < 4; i++) {
        uint16_t h = PAD_ButtonsHeld(i);
        b |= h & 0x1F7F;
        int sx = PAD_StickX(i), sy = PAD_StickY(i), cx = PAD_SubStickX(i), cy = PAD_SubStickY(i);
        if (sx < -48) b |= BTN_LEFT;
        if (sx > 48) b |= BTN_RIGHT;
        if (sy > 48) b |= BTN_UP;
        if (sy < -48) b |= BTN_DOWN;
        if (cx < -48) b |= BTN_CLEFT;
        if (cx > 48) b |= BTN_CRIGHT;
        if (cy > 48) b |= BTN_CUP;
        if (cy < -48) b |= BTN_CDOWN;
    }
    return b;
}

void input_update(void) {
    held = read_pad() | injected;
    injected = 0;
    pressed = held & ~prev;
    const uint32_t dirs = BTN_LEFT | BTN_RIGHT | BTN_UP | BTN_DOWN | BTN_CLEFT | BTN_CRIGHT | BTN_CUP | BTN_CDOWN;
    if (held & dirs) {
        if (pressed & dirs) repeat_timer = 18;
        else if (--repeat_timer <= 0) { pressed |= held & dirs; repeat_timer = 5; }
    }
    idle = held ? 0 : idle + 1;
    prev = held;
}

uint32_t input_pressed(void) { return pressed; }
uint32_t input_held(void) { return held; }
int input_idle_frames(void) { return idle; }
void input_inject(uint32_t b) { injected |= b; }
