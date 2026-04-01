#include "audio.h"
#include "game.h"
#include <algorithm>
#include <cstdlib>

static float clampf(float v) { return std::clamp(v, 0.0f, 1.0f); }
static float effective(const AudioState *a) { return a->master_volume * a->sfx_volume; }

// Random float in [lo, hi]
static float randf(float lo, float hi)
{
    return lo + (hi - lo) * ((float)std::rand() / (float)RAND_MAX);
}

void audio_init(AudioState *a)
{
    InitAudioDevice();
    a->snd_walk   = LoadSound(assets_path("sounds/walk.mp3").c_str());
    a->snd_run    = LoadSound(assets_path("sounds/run.mp3").c_str());
    a->snd_land   = LoadSound(assets_path("sounds/land.mp3").c_str());
    a->snd_attack = LoadSound(assets_path("sounds/attack.mp3").c_str());
}

void audio_cleanup(AudioState *a)
{
    UnloadSound(a->snd_walk);
    UnloadSound(a->snd_run);
    UnloadSound(a->snd_land);
    UnloadSound(a->snd_attack);
    CloseAudioDevice();
}

void audio_set_master_volume(AudioState *a, float v) { a->master_volume = clampf(v); }
void audio_set_sfx_volume   (AudioState *a, float v) { a->sfx_volume    = clampf(v); }
void audio_set_music_volume  (AudioState *a, float v) { a->music_volume  = clampf(v); }
float audio_get_master_volume(const AudioState *a) { return a->master_volume; }
float audio_get_sfx_volume   (const AudioState *a) { return a->sfx_volume; }
float audio_get_music_volume  (const AudioState *a) { return a->music_volume; }

void audio_play_sfx(AudioState *a, Sound &snd)
{
    if (IsSoundPlaying(snd)) return;
    SetSoundVolume(snd, effective(a));
    PlaySound(snd);
}

void audio_play_footstep(AudioState *a, Sound &snd)
{
    StopSound(snd);
    SetSoundVolume(snd, effective(a));
    PlaySound(snd);
}

void audio_play_sfx_pitched(AudioState *a, Sound &snd, float pitch_min, float pitch_max)
{
    StopSound(snd); // stop any lingering instance before re-pitching
    SetSoundPitch(snd, randf(pitch_min, pitch_max));
    SetSoundVolume(snd, effective(a));
    PlaySound(snd);
}
