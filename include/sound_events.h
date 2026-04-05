#pragma once

#include "raylib.h"
#include <vector>

struct SoundEvent {
    Vector2 position{};
    float   radius   = 0.0f;
    float   lifetime = 0.0f;
    float   age      = 0.0f;
};

struct SoundEventSystem {
    std::vector<SoundEvent> events;
};

// Register a short-lived sound cue that nearby AI can react to.
void sound_events_push(SoundEventSystem *system, Vector2 position, float radius, float lifetime);

// Advance event ages and remove expired entries.
void sound_events_update(SoundEventSystem *system, float dt);

// Clear all active sound cues, e.g. on level load.
void sound_events_clear(SoundEventSystem *system);
