#include "sound_events.h"
#include <algorithm>

static constexpr int SOUND_EVENT_CAP = 48;
static float g_footstep_radius = 90.0f;
static float g_rifle_radius    = 220.0f;
static float g_deagle_radius   = 280.0f;
static float g_impact_radius   = 150.0f;

static float clamp_sound_radius(float radius)
{
    return std::clamp(radius, 8.0f, 2000.0f);
}

void sound_events_push(SoundEventSystem *system, Vector2 position, float radius, float lifetime,
                       SoundEventType type)
{
    if (!system || radius <= 0.0f || lifetime <= 0.0f) return;

    if ((int)system->events.size() >= SOUND_EVENT_CAP)
        system->events.erase(system->events.begin());

    SoundEvent event;
    event.position = position;
    event.radius   = radius;
    event.lifetime = lifetime;
    event.type     = type;
    system->events.push_back(event);
}

void sound_events_update(SoundEventSystem *system, float dt)
{
    if (!system) return;

    for (SoundEvent& event : system->events)
        event.age += dt;

    system->events.erase(
        std::remove_if(system->events.begin(), system->events.end(),
                       [](const SoundEvent& event) { return event.age >= event.lifetime; }),
        system->events.end());
}

void sound_events_clear(SoundEventSystem *system)
{
    if (!system) return;
    system->events.clear();
}

float sound_events_get_footstep_radius() { return g_footstep_radius; }
void sound_events_set_footstep_radius(float radius) { g_footstep_radius = clamp_sound_radius(radius); }

float sound_events_get_rifle_radius() { return g_rifle_radius; }
void sound_events_set_rifle_radius(float radius) { g_rifle_radius = clamp_sound_radius(radius); }

float sound_events_get_deagle_radius() { return g_deagle_radius; }
void sound_events_set_deagle_radius(float radius) { g_deagle_radius = clamp_sound_radius(radius); }

float sound_events_get_impact_radius() { return g_impact_radius; }
void sound_events_set_impact_radius(float radius) { g_impact_radius = clamp_sound_radius(radius); }
