#pragma once

#include "raylib.h"
#include "tilemap.h"
#include "bullets.h"
#include "weapon.h"
#include "audio.h"
#include <memory>
#include <vector>

// Rectangular collision box dimensions, centred on enemy.position.
// Generous by design (Hotline Miami-style) — shots that look like hits, are hits.
inline constexpr float ENEMY_HITBOX_W = 40.0f;
inline constexpr float ENEMY_HITBOX_H = 60.0f;

enum class EnemyAIState {
    IDLE,    // standing still, watching
    ALERT,   // saw player — reaction delay before moving
    CHASE,   // moving toward player
    ATTACK,  // within range, burst-firing
    DEAD
};

struct Enemy {
    Vector2      position{};
    bool         alive       = true;
    int          sprite_row  = 0;   // idle sheet row (0=front … 4=back)
    bool         sprite_flip = false;

    // ── AI ────────────────────────────────────────────────────────────
    EnemyAIState ai_state       = EnemyAIState::IDLE;
    float        alert_timer    = 0.0f; // reaction delay in ALERT
    float        lost_sight_timer = 0.0f; // how long since we last saw the player

    // Burst fire
    int   burst_remaining  = 0;
    float burst_timer      = 0.0f; // pause between bursts
    float fire_cooldown    = 0.0f; // per-shot cooldown within burst

    // Facing / aim
    float   facing_angle = 0.0f;  // screen degrees (0=right, 90=down)
    Vector2 aim_dir      = { 1.0f, 0.0f };

    // ── Weapon ────────────────────────────────────────────────────────
    std::unique_ptr<Weapon> weapon; // owned exclusively; not in WeaponManager
    // Weapon render state (mirrors Weapon runtime fields used by draw code)
    float   weapon_render_rotation = 0.0f;
    Vector2 weapon_render_pos{};
    float   weapon_recoil_offset   = 0.0f;
};

struct EnemyManager {
    Texture2D          sprite_sheet{};
    std::vector<Enemy> enemies;
};

// Returns the axis-aligned collision rectangle for an enemy, centred on enemy.position.
Rectangle enemy_hitbox_rect(const Enemy *enemy);

// Load the enemy sprite sheet. Call once after InitWindow.
void enemies_init(EnemyManager *em);

// Populate enemies from tilemap point objects whose name is "enemy".
// Reads the optional string property "facing" ("up","down","left","right").
// Each enemy receives a random weapon (assault_rifle or deagle).
void enemies_load_from_tilemap(EnemyManager *em, const Tilemap *tm);

// Remove all enemies without unloading the sprite sheet.
void enemies_clear(EnemyManager *em);

// Run AI state machine, vision, movement, and weapon fire.
void enemies_update(EnemyManager *em, BulletSystem *bullets, AudioState *audio,
                    const Tilemap *tm, Vector2 player_center, float dt);

// Draw all alive enemies and their weapons.
void enemies_draw(const EnemyManager *em);

// Unload enemy resources.
void enemies_cleanup(EnemyManager *em);
