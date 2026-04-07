#pragma once

#include "raylib.h"
#include <vector>

enum class SoundEventType {
    GENERIC,
    FOOTSTEP,
    GUNSHOT,
    IMPACT
};

struct SoundEvent {
    Vector2 position{};
    float   radius   = 0.0f;
    float   lifetime = 0.0f;
    float   age      = 0.0f;
    SoundEventType type = SoundEventType::GENERIC;
};

struct SoundEventSystem {
    std::vector<SoundEvent> events;
};

// Register a short-lived sound cue that nearby AI can react to.
void sound_events_push(SoundEventSystem *system, Vector2 position, float radius, float lifetime,
                       SoundEventType type = SoundEventType::GENERIC);

// Advance event ages and remove expired entries.
void sound_events_update(SoundEventSystem *system, float dt);

// Clear all active sound cues, e.g. on level load.
void sound_events_clear(SoundEventSystem *system);

float sound_events_get_footstep_radius();
void sound_events_set_footstep_radius(float radius);

float sound_events_get_rifle_radius();
void sound_events_set_rifle_radius(float radius);

float sound_events_get_deagle_radius();
void sound_events_set_deagle_radius(float radius);

float sound_events_get_impact_radius();
void sound_events_set_impact_radius(float radius);
