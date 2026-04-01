#pragma once

#include "raylib.h"
#include <vector>

struct Bullet {
    Vector2 position{};
    Vector2 velocity{};
    float   age = 0.0f;
    float   lifetime = 0.0f;
    float   frame_timer = 0.0f;
    int     frame_index = 0;
};

struct BulletSystem {
    Texture2D texture{};
    std::vector<Bullet> bullets;
};

// Load the bullet spritesheet and initialise runtime state.
void bullets_init(BulletSystem *system);

// Spawn a bullet travelling in direction from origin.
void bullets_spawn(BulletSystem *system, Vector2 origin, Vector2 direction);

// Remove all live bullets without unloading the spritesheet.
void bullets_clear(BulletSystem *system);

// Advance bullet simulation and remove expired bullets.
void bullets_update(BulletSystem *system, float dt);

// Draw every live bullet.
void bullets_draw(const BulletSystem *system);

// Release bullet resources.
void bullets_cleanup(BulletSystem *system);

// FUTURE: to add bullet-enemy collision, iterate system->bullets and check each
// bullet.position against enemy hitboxes. Remove hits via bullets_clear or erase-remove.
// Bullet radius for overlap: approximately BULLET_FRAME_SIZE * BULLET_SCALE * 0.5f.
