#pragma once

#include "raylib.h"
#include "tilemap.h"
#include <vector>

// Circle radius used for bullet-vs-enemy hit detection.
inline constexpr float ENEMY_RADIUS = 16.0f;

struct Enemy {
    Vector2 position{};      // world-space center (matches Tiled point object position)
    bool    alive      = true;
    int     sprite_row = 0;  // idle sheet row (0=front … 4=back)
    bool    sprite_flip = false;
};

struct EnemyManager {
    Texture2D          sprite_sheet{};  // shared by all enemies; swap for a real sprite later
    std::vector<Enemy> enemies;
};

// Load the enemy sprite sheet. Call once after InitWindow.
void enemies_init(EnemyManager *em);

// Populate enemies from tilemap point objects whose name is "enemy".
// Reads the optional string property "facing" ("up", "down", "left", "right").
// Call after every tilemap load to replace the previous level's enemies.
void enemies_load_from_tilemap(EnemyManager *em, const Tilemap *tm);

// Remove all enemies without unloading the sprite sheet.
void enemies_clear(EnemyManager *em);

// Called once per frame. Currently a no-op (enemies are static).
// FUTURE: move enemies, handle attack cooldowns, run AI here.
void enemies_update(EnemyManager *em, float dt);

// Draw all alive enemies.
void enemies_draw(const EnemyManager *em);

// Unload enemy resources.
void enemies_cleanup(EnemyManager *em);
