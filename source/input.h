#pragma once
#include <stdint.h>

/* GameCube buttons, with the analog stick folded into the d-pad and auto-repeat on directions */
enum {
    BTN_LEFT = 0x0001, BTN_RIGHT = 0x0002, BTN_DOWN = 0x0004, BTN_UP = 0x0008,
    BTN_Z = 0x0010, BTN_R = 0x0020, BTN_L = 0x0040,
    BTN_A = 0x0100, BTN_B = 0x0200, BTN_X = 0x0400, BTN_Y = 0x0800, BTN_START = 0x1000,
    BTN_CLEFT = 0x10000, BTN_CRIGHT = 0x20000, BTN_CUP = 0x40000, BTN_CDOWN = 0x80000,
};

void input_init(void);
void input_update(void);
uint32_t input_pressed(void);   /* edge + repeat */
uint32_t input_held(void);
int input_idle_frames(void);
void input_inject(uint32_t buttons);   /* demo/tour scripts */
