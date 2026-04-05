#include "sound_events.h"
#include <algorithm>

static constexpr int SOUND_EVENT_CAP = 48;

void sound_events_push(SoundEventSystem *system, Vector2 position, float radius, float lifetime)
{
    if (!system || radius <= 0.0f || lifetime <= 0.0f) return;

    if ((int)system->events.size() >= SOUND_EVENT_CAP)
        system->events.erase(system->events.begin());

    SoundEvent event;
    event.position = position;
    event.radius   = radius;
    event.lifetime = lifetime;
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
