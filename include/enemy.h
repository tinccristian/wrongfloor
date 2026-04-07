#pragma once

#include "raylib.h"
#include "tilemap.h"
#include "bullets.h"
#include "weapon.h"
#include "audio.h"
#include "animation.h"
#include "sound_events.h"
#include <memory>
#include <vector>

// Rectangular collision box dimensions, centred on enemy.position.
// Generous by design (Hotline Miami-style) — shots that look like hits, are hits.
inline constexpr float ENEMY_HITBOX_W = 40.0f;
inline constexpr float ENEMY_HITBOX_H = 60.0f;

enum class EnemyAIState {
    IDLE,    // standing still, watching
    ALERT,   // saw player — reaction delay before acting
    CHASE,   // moving toward player
    SEARCH,  // investigating a last known or heard position
    ATTACK,  // within range, firing continuously
    DEAD
};

struct Enemy {
    Vector2      position{};
    bool         alive = true;

    // ── AI ────────────────────────────────────────────────────────────
    EnemyAIState ai_state    = EnemyAIState::IDLE;
    float        alert_timer = 0.0f; // reaction delay in ALERT
    float        search_timer = 0.0f;
    float        time_since_last_seen_player = 0.0f;
    float        hearing_cooldown = 0.0f;
    bool         can_currently_see_player = false;

    // Per-shot cooldown (replaces burst pattern).
    float fire_cooldown = 0.0f;

    Vector2 last_known_player_pos{};
    bool    has_last_known_player_pos = false;

    Vector2 investigation_target{};
    bool    has_investigation_target = false;

    // Facing / aim — updated each frame once alerted; stays at spawn dir in IDLE.
    float   facing_angle = 0.0f; // screen degrees (0=right, 90=down)
    Vector2 aim_dir      = { 1.0f, 0.0f };

    // ── Weapon ────────────────────────────────────────────────────────
    std::unique_ptr<Weapon> weapon; // owned exclusively; not in WeaponManager
    float   weapon_render_rotation = 0.0f;
    Vector2 weapon_render_pos{};
    float   weapon_recoil_offset   = 0.0f;

    // ── Animation ─────────────────────────────────────────────────────
    AnimationPlayer anim_player{};  // points into EnemyManager animation arrays
};

struct EnemyManager {
    // Sprite sheets — shared across all enemies (non-owning ptrs in Animation structs
    // point into these).
    Texture2D sprite_sheet{};  // idle sheet: character/idle.png
    Texture2D run_sheet{};     // run sheet:  character/run.png

    // Animation banks: 5 directional rows for idle and run, matching the player layout.
    static constexpr int ANIM_ROW_COUNT = 5;
    Animation anim_idle_rows[ANIM_ROW_COUNT]{};
    Animation anim_run_rows [ANIM_ROW_COUNT]{};

    std::vector<Enemy> enemies;
};

// Returns the axis-aligned collision rectangle for an enemy, centred on enemy.position.
Rectangle enemy_hitbox_rect(const Enemy *enemy);

// Load the enemy sprite sheets and initialise animation banks. Call once after InitWindow.
void enemies_init(EnemyManager *em);

// Populate enemies from tilemap point objects whose name is "enemy".
// Reads the optional string property "facing" ("up","down","left","right").
// Each enemy receives a random weapon (assault_rifle or deagle).
void enemies_load_from_tilemap(EnemyManager *em, const Tilemap *tm);

// Remove all enemies without unloading sprite sheets.
void enemies_clear(EnemyManager *em);

// Forward declarations needed for the new parameters.
struct Player;
struct EffectsSystem;

// Run AI state machine, vision, movement, and weapon fire. Update animations.
// player and effects are used for melee-vs-player hit detection.
// Returns true if the player was killed by an enemy melee weapon this frame.
bool enemies_update(EnemyManager *em, BulletSystem *bullets, AudioState *audio,
                    const Tilemap *tm, SoundEventSystem *sound_events,
                    Vector2 player_center, float dt,
                    Player *player = nullptr, EffectsSystem *effects = nullptr);

// Draw all alive enemies and their weapons.
void enemies_draw(const EnemyManager *em);

const char* enemy_ai_state_name(EnemyAIState state);
float enemies_get_search_time();
void enemies_set_search_time(float seconds);
float enemies_get_memory_time();
void enemies_set_memory_time(float seconds);
void enemies_reset_perception(EnemyManager *em);

// Unload enemy resources.
void enemies_cleanup(EnemyManager *em);
