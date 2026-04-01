#pragma once

#include "raylib.h"

struct AudioState {
    Sound snd_walk;
    Sound snd_run;
    Sound snd_land;
    Sound snd_attack;

    float master_volume = 0.8f;
    float sfx_volume    = 1.0f;
    float music_volume  = 1.0f; // reserved for future BGM
};

// Call once after InitWindow — initialises the device and loads all sounds.
void audio_init(AudioState *a);

// Call once before CloseWindow.
void audio_cleanup(AudioState *a);

// Volume controls — each clamps to [0, 1].
void audio_set_master_volume(AudioState *a, float v);
void audio_set_sfx_volume(AudioState *a, float v);
void audio_set_music_volume(AudioState *a, float v);
float audio_get_master_volume(const AudioState *a);
float audio_get_sfx_volume(const AudioState *a);
float audio_get_music_volume(const AudioState *a);

// Play a sound at current effective (master * sfx) volume.
// Skips if the sound is already playing (prevents rapid-fire overlap).
void audio_play_sfx(AudioState *a, Sound &snd);

// Play a footstep sound: always restarts from the beginning on each footstep frame.
// Use this instead of audio_play_sfx for walk/run footsteps.
void audio_play_footstep(AudioState *a, Sound &snd);

// Same but randomises pitch in [pitch_min, pitch_max] first.
// Always plays (does not skip if already playing) — suitable for one-shot events.
void audio_play_sfx_pitched(AudioState *a, Sound &snd, float pitch_min, float pitch_max);
