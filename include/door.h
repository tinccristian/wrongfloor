#pragma once
#include "raylib.h"
#include "tilemap.h"
#include <vector>

// Forward declarations — keeps this header lightweight.
struct Player;
struct EnemyManager;
struct WeaponManager;
struct BulletSystem;
struct EffectsSystem;

struct Door {
    Vector2 hinge_position{};
    float   length           = 48.0f;   // world-px: the long dimension
    float   thickness        = 6.0f;    // world-px: the short dimension
    float   angle            = 0.0f;    // current rotation in radians (starts at closed_angle)
    float   closed_angle     = 0.0f;    // resting / closed orientation in radians
    float   angular_velocity = 0.0f;    // radians/second; decays via drag each frame
    float   slam_kill_timer  = 0.0f;   // counts down from 0.5s after a slam; kills enemies while >0
    bool    is_open          = false;   // true while resting >45° from closed_angle
};

struct DoorSystem {
    std::vector<Door> doors;
};

// Parse "door" rectangle objects from the tilemap and populate ds->doors.
// Each object must have a "hinge" string property: "start" or "end".
// Orientation (horizontal vs vertical) is inferred from width vs height.
void doors_load_from_tilemap(DoorSystem *ds, const Tilemap *tm);

// Remove all doors (no GPU resources to release).
void doors_clear(DoorSystem *ds);

// Advance door physics; resolve contacts with player, enemies, bullets, and thrown weapons.
// Returns true if the player was slam-killed by a door this frame (caller triggers death).
bool doors_update(DoorSystem *ds, Player *player, EnemyManager *enemies,
                  BulletSystem *bullets, WeaponManager *weapons,
                  EffectsSystem *effects, float dt);

// Draw all doors as filled rotated rectangles with a lighter border edge.
void doors_draw(const DoorSystem *ds);
