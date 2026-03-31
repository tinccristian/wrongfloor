#pragma once

#include "raylib.h"

static constexpr int MAX_ANIMATION_FRAMES = 16;

// A single loopable or one-shot animation backed by an array of source rectangles.
// The texture pointer is non-owning — the caller manages the texture lifetime.
struct Animation {
    Texture2D *texture;
    Rectangle  frames[MAX_ANIMATION_FRAMES];
    int        frame_count;
    float      frame_duration; // seconds per frame
    bool       loops;
};

// Runtime state for playing an Animation.
struct AnimationPlayer {
    const Animation *current;
    int   frame_index;
    float elapsed;
    bool  flip_h; // flip sprite horizontally when true
};

// Switch to anim; resets playback only if the animation actually changes.
void animation_player_set(AnimationPlayer *ap, const Animation *anim);

// Advance playback by dt seconds.
void animation_player_update(AnimationPlayer *ap, float dt);

// Draw the current frame at position (top-left) scaled by scale.
void animation_player_draw(const AnimationPlayer *ap, Vector2 position, float scale);
