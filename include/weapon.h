#pragma once

#include "raylib.h"
#include "tilemap.h"
#include "bullets.h"
#include "audio.h"
#include <string>
#include <vector>

// ── Blueprint: all gun-specific constants ─────────────────────────────────────
// Adding a new gun type = defining one more WeaponData in weapons_init().
struct WeaponData {
    std::string name;
    Texture2D   texture{};          // held-weapon sprite
    Texture2D   outline_texture{};  // pre-outlined sprite used when the weapon is on the ground
    int         sprite_width  = 35;
    int         sprite_height = 11;
    float       render_scale  = 2.0f;   // world px per sprite px
    float       fire_rate     = 10.0f;  // shots per second
    int         magazine_size = 30;
    float       reload_time   = 1.5f;   // seconds
    float       bullet_speed  = 900.0f; // px/sec
    int         bullet_damage = 1;
    float       spread_angle  = 4.0f;   // degrees, half-width of random spread cone
    bool        throwable     = true;
    bool        auto_fire     = true;   // hold to keep firing

    // Sounds — loaded in weapons_init, unloaded in weapons_cleanup.
    Sound snd_shot{};           // base shot sound (also source for shot_aliases)
    Sound snd_reload{};         // reload start sound
    Sound shot_aliases[4]{};    // aliases for overlapping rapid fire
};

// ── Instance: one physical weapon in the world ───────────────────────────────
struct Weapon {
    const WeaponData *data = nullptr;   // non-owning; pointer into WeaponManager::weapon_datas

    Vector2 position{};         // world-space center (used when not held)
    int     current_ammo = 0;

    bool  is_held      = false;
    bool  is_on_ground = true;
    bool  is_thrown    = false;
    bool  is_reloading = false;
    float reload_timer = 0.0f;
    float fire_cooldown_timer = 0.0f;

    Vector2 throw_velocity{};   // active while is_thrown

    // Visual/animation state
    float bob_timer       = 0.0f;  // sine phase for ground bobbing
    float recoil_offset   = 0.0f;  // px backward kick, lerps back to 0 after shot
    float render_rotation = 0.0f;  // degrees, smoothly lerped toward aim angle
    Vector2 render_pos{};          // computed from render_rotation each frame when held
    int   shot_alias_idx  = 0;     // cycles through data->shot_aliases[] for overlap
};

// ── Manager: type catalog + all live instances ────────────────────────────────
struct WeaponManager {
    std::vector<WeaponData> weapon_datas;   // catalog — populated once in weapons_init
    std::vector<Weapon>     weapons;        // all instances (ground, held, thrown)
    bool trigger_was_pressed = false;       // tracks controller right-trigger edge
};

// Load weapon textures and register all gun types. Call once after InitWindow.
void weapons_init(WeaponManager *wm);

// Spawn a weapon of the named type on the ground at position. Returns index, or -1.
int weapons_spawn(WeaponManager *wm, const std::string& type_name, Vector2 position);

// Read objects with name or class "weapon" from the tilemap and spawn them.
void weapons_load_from_tilemap(WeaponManager *wm, const Tilemap *tm);

// Remove all weapon instances without unloading textures (call on level load).
void weapons_clear(WeaponManager *wm);

// Advance physics, handle pickup/throw/fire/reload input, spawn bullets.
// player_center and aim_direction come directly from the Player struct.
void weapons_update(WeaponManager *wm, BulletSystem *bullets, AudioState *audio,
                    const Tilemap *tm, Vector2 player_center, Vector2 aim_direction,
                    bool input_blocked, float dt);

// Draw all ground and thrown weapons with white outline and bob.
// player_center is used to show the pickup prompt when in range.
// controller_active: pass true to show controller button, false for keyboard key.
void weapons_draw_ground(const WeaponManager *wm, Vector2 player_center, bool controller_active);

// Draw the held weapon on top of the player (always call after player_draw).
void weapons_draw_held(const WeaponManager *wm);

// Draw world-space HUD: reload bar above player. Call inside BeginMode2D.
void weapons_draw_hud(const WeaponManager *wm, Vector2 player_center);

// Draw screen-space HUD: ammo counter at bottom-right. Call outside BeginMode2D.
void weapons_draw_ammo_screen(const WeaponManager *wm, int screen_w, int screen_h);

// Unload all weapon data textures. Call once before CloseWindow.
void weapons_cleanup(WeaponManager *wm);

// True if the player currently holds any weapon.
bool weapons_player_has_weapon(const WeaponManager *wm);
