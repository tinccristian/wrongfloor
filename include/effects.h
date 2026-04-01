#pragma once

#include "raylib.h"
#include <vector>

struct BloodParticle {
    Vector2 position{};
    Vector2 velocity{};
    float   lifetime = 0.5f;
    float   age      = 0.0f;
    float   size     = 4.0f;  // starting radius in pixels
    Color   color{};
};

struct BloodStain {
    Vector2 position{};
    float   radius = 3.0f;
    Color   color{};
};

struct EffectsSystem {
    std::vector<BloodParticle> particles;
    std::vector<BloodStain>    stains;   // persist for the remainder of the level
};

// Spawn a blood burst at position, biased in bullet_direction (normalised or zero).
// Creates both transient particles and persistent stains.
void effects_spawn_blood(EffectsSystem *effects, Vector2 position, Vector2 bullet_direction);

// Advance particle simulation and remove expired particles.
void effects_update(EffectsSystem *effects, float dt);

// Draw persistent blood stains. Call before entities (after midground tiles).
void effects_draw_stains(const EffectsSystem *effects);

// Draw active particles. Call after entities (before foreground tiles).
void effects_draw_particles(const EffectsSystem *effects);

// Remove all particles and stains. Call on level transition.
void effects_clear(EffectsSystem *effects);
