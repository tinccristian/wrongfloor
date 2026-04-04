#pragma once

#include "raylib.h"
#include "tilemap.h"
#include <vector>

// A single blood pixel in flight. Settles when speed drops below threshold or hits a wall.
struct BloodPixel {
    Vector2 position{};
    Vector2 velocity{};
    float   age          = 0.0f;
    float   max_lifetime = 3.0f; // safety cap; pixels normally settle via speed long before this
    int     size         = 3;    // world-space pixel edge length — uniform for art consistency
    Color   color{};
    bool    is_spear     = false; // high-velocity pixel; leaves wall smear on impact
    bool    settled      = false;
};

// A permanent blood mark left on the ground. Never updated — draw only.
struct SettledPixel {
    Vector2 position{};
    int     size = 1;
    Color   color{};
};

struct EffectsSystem {
    std::vector<BloodPixel>   pixels;   // in-flight; updated every frame
    std::vector<SettledPixel> stains;   // permanent; accumulated for the level duration
};

// Spawn a blood burst at position, biased strongly in bullet_direction.
// Spawns splatter pixels (immediate fast burst) and registers ooze sources
// (slow trickle for ~1-2 s afterward that pools into a stain).
void effects_spawn_blood(EffectsSystem *effects, Vector2 position, Vector2 bullet_direction);

// Larger, asymmetric burst for player death: main splatter in bullet direction
// plus several upward pump jets simulating a heartbeat under pressure.
// Call once on hit — the fountain system sustains the spray afterward.
void effects_spawn_player_death_blood(EffectsSystem *effects, Vector2 position,
                                      Vector2 bullet_direction);

// Advance pixel physics, handle wall settling (via tilemap collision),
// and emit from active ooze sources. Settled pixels are transferred to stains.
void effects_update(EffectsSystem *effects, const Tilemap *tm, float dt);

// Draw permanent blood stains. Call before entities (after midground tiles).
void effects_draw_stains(const EffectsSystem *effects);

// Draw in-flight blood pixels. Call after entities (before foreground tiles).
void effects_draw_pixels(const EffectsSystem *effects);

// Remove all pixels, stains, and ooze sources. Call on level transition.
void effects_clear(EffectsSystem *effects);

// Active in-flight pixel count and total settled stain count (for debug display).
int effects_active_count(const EffectsSystem *effects);
int effects_stain_count(const EffectsSystem *effects);
