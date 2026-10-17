#pragma once
#include <stdbool.h>

enum { SFX_MOVE, SFX_SELECT, SFX_BACK, SFX_OPEN, SFX_LAUNCH, SFX_COUNT };

void audio_init(void);
void audio_update(float dt);          /* fades, menu music rotation */
void audio_music(const char *path);   /* game theme; NULL = back to menu music */
void audio_volume(int vol10);
void audio_sfx(int id);
void audio_stop(void);
const char *audio_now_playing(void);
