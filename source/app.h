#pragma once
#include <stdbool.h>

enum { SCR_INTRO, SCR_LIBRARY, SCR_DETAILS, SCR_SETTINGS, SCR_SYSTEM, SCR_LAUNCH };

void app_init(void);
void app_frame(float dt);     /* input + draw one frame */
bool app_demo_active(void);
