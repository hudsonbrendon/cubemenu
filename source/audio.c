#include "audio.h"
#include "fs.h"
#include "settings.h"
#include <gccore.h>
#include <aesndlib.h>
#include <mp3player.h>
#include <dirent.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static FILE *mf;
static char cur[160], next_path[160];
static bool next_pending;
static float fade = 1, target = 1;
static int vol = 255;
static char menu_tracks[16][96];
static int n_menu, menu_idx;
static bool playing_menu;

static AESNDPB *voice;
static short *sfx_pcm[SFX_COUNT];
static int sfx_len[SFX_COUNT];
#define SFX_RATE 32000
static float clampf_(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static s32 reader(void *cb, void *dst, s32 len) {
    (void)cb;
    if (!mf) return -1;
    size_t n = fread(dst, 1, len, mf);
    return n ? (s32)n : -1;
}

static void apply_volume(void) { MP3Player_Volume((u32)(vol * fade)); }

static void start(const char *path) {
    MP3Player_Stop();
    if (mf) { fclose(mf); mf = NULL; }
    cur[0] = 0;
    if (!path || !path[0]) return;
    mf = fopen(path, "rb");
    if (!mf) return;
    snprintf(cur, sizeof cur, "%s", path);
    fade = 0; target = 1;
    apply_volume();
    MP3Player_PlayFile(NULL, reader, NULL);
}

/* short synthesized blips in the spirit of the GameCube menu */
static void synth(int id, float f0, float f1, float dur, float vol, int harmonics) {
    int n = (int)(SFX_RATE * dur) & ~15;
    short *p = memalign(32, n * 2);
    for (int i = 0; i < n; i++) {
        float t = (float)i / SFX_RATE, k = (float)i / n;
        float f = f0 + (f1 - f0) * k;
        float env = (k < 0.04f ? k / 0.04f : 1.0f) * expf(-4.5f * k);
        float s = sinf(2 * M_PI * f * t);
        if (harmonics) s = 0.7f * s + 0.3f * sinf(2 * M_PI * f * 2.01f * t);
        p[i] = (short)(s * env * vol * 32767);
    }
    DCFlushRange(p, n * 2);
    sfx_pcm[id] = p; sfx_len[id] = n * 2;
}

/* boot chime: a quick bright arpeggio that settles on a soft chord */
static void synth_chime(int id) {
    static const float notes[] = {523.25f, 659.25f, 783.99f, 1046.5f, 1318.5f};
    const float dur = 1.6f;
    int n = (int)(SFX_RATE * dur) & ~15;
    short *p = memalign(32, n * 2);
    for (int i = 0; i < n; i++) {
        float t = (float)i / SFX_RATE, s = 0;
        for (int k = 0; k < 5; k++) {
            float t0 = k * 0.075f;
            if (t < t0) continue;
            float tt = t - t0, env = (tt < 0.01f ? tt / 0.01f : 1.0f) * expf(-2.6f * tt);
            s += env * (sinf(2 * M_PI * notes[k] * tt) + 0.25f * sinf(2 * M_PI * notes[k] * 2.0f * tt)) * 0.22f;
        }
        p[i] = (short)(clampf_(s, -1, 1) * 32767 * 0.6f);
    }
    DCFlushRange(p, n * 2);
    sfx_pcm[id] = p; sfx_len[id] = n * 2;
}

static void scan_menu_music(void) {
    char path[96];
    DIR *d = opendir(fs_path(path, sizeof path, "/cubemenu/music"));
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && n_menu < 16) {
        size_t l = strlen(e->d_name);
        if (e->d_name[0] == '.' || l < 5 || strcasecmp(e->d_name + l - 4, ".mp3")) continue;
        snprintf(menu_tracks[n_menu++], sizeof menu_tracks[0], "%s/%s", path, e->d_name);
    }
    closedir(d);
}

void audio_init(void) {
    AESND_Init();
    MP3Player_Init();
    voice = AESND_AllocateVoice(NULL);
    synth(SFX_MOVE, 1180, 1320, 0.07f, 0.22f, 0);
    synth(SFX_SELECT, 880, 1760, 0.16f, 0.30f, 1);
    synth(SFX_BACK, 700, 420, 0.14f, 0.26f, 1);
    synth(SFX_OPEN, 520, 1040, 0.22f, 0.28f, 1);
    synth(SFX_LAUNCH, 330, 990, 0.9f, 0.32f, 1);
    synth_chime(SFX_INTRO);
    scan_menu_music();
}

void audio_volume(int v) { vol = v * 255 / 10; apply_volume(); }

void audio_sfx(int id) {
    if (!g_set.sfx || !voice || id < 0 || id >= SFX_COUNT) return;
    int v = vol;
    AESND_SetVoiceVolume(voice, v, v);
    AESND_PlayVoice(voice, VOICE_MONO16, sfx_pcm[id], sfx_len[id], SFX_RATE, 0, false);
}

void audio_music(const char *path) {
    const char *want = "";
    playing_menu = false;
    if (path && path[0]) want = path;
    else if (g_set.menu_music && n_menu) { want = menu_tracks[menu_idx % n_menu]; playing_menu = true; }
    if (!strcmp(want, cur) && MP3Player_IsPlaying()) { target = 1; next_pending = false; return; }
    if (cur[0] && MP3Player_IsPlaying()) {       /* fade out first */
        snprintf(next_path, sizeof next_path, "%s", want);
        next_pending = true;
        target = 0;
    } else start(want);
}

void audio_update(float dt) {
    if (fade != target) {
        float step = dt / (target > fade ? 0.8f : 0.35f);
        fade = target > fade ? fminf(target, fade + step) : fmaxf(target, fade - step);
        apply_volume();
    }
    if (next_pending && fade <= 0.001f) { next_pending = false; start(next_path); }
    if (!next_pending && cur[0] && !MP3Player_IsPlaying()) {
        if (playing_menu && n_menu) { menu_idx++; start(menu_tracks[menu_idx % n_menu]); }
        else { char again[160]; snprintf(again, sizeof again, "%s", cur); start(again); }   /* themes loop */
    }
}

const char *audio_now_playing(void) { return cur; }

void audio_stop(void) {
    MP3Player_Stop();
    if (mf) { fclose(mf); mf = NULL; }
    if (voice) AESND_SetVoiceStop(voice, true);
    AESND_Pause(true);
}
