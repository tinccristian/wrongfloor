#pragma once

#include "weapon.h"
#include "tilemap.h"
#include "bullets.h"
#include "audio.h"
#include <memory>
#include <string_view>
#include <vector>

// Owns all live weapon instances and drives shared behavior through the Weapon interface.
struct WeaponManager {
    std::vector<std::unique_ptr<Weapon>> weapons;
    bool trigger_was_pressed = false;  // tracks controller right-trigger edge
};

// Initialise the system. Currently a no-op — weapons are spawned on level load.
// Call once after InitWindow.
void weapons_init(WeaponManager *wm);

// Spawn a weapon of the named type on the ground at position. Returns index, or -1.
// To add a new type: create the subclass and add an else-if in weapon_manager.cpp.
int weapons_spawn(WeaponManager *wm, std::string_view type_name, Vector2 position);

// Read objects with name or class "weapon" from the tilemap and spawn them.
void weapons_load_from_tilemap(WeaponManager *wm, const Tilemap *tm);

// Remove all weapon instances. Unique_ptrs destroy the weapons (and their assets).
void weapons_clear(WeaponManager *wm);

// Advance physics, handle pickup/throw/fire/reload input, spawn bullets.
// player_center and aim_direction come directly from the Player struct.
void weapons_update(WeaponManager *wm, BulletSystem *bullets, AudioState *audio,
                    const Tilemap *tm, Vector2 player_center, Vector2 aim_direction,
                    bool input_blocked, float dt);

// Draw all ground and thrown weapons with outline and bob.
// controller_active: pass true to show controller button, false for keyboard key.
void weapons_draw_ground(const WeaponManager *wm, Vector2 player_center, bool controller_active);

// Draw the held weapon on top of the player (always call after player_draw).
void weapons_draw_held(const WeaponManager *wm);

// Draw world-space HUD: reload bar above player. Call inside BeginMode2D.
void weapons_draw_hud(const WeaponManager *wm, Vector2 player_center);

// Draw screen-space HUD: ammo counter at bottom-right. Call outside BeginMode2D.
void weapons_draw_ammo_screen(const WeaponManager *wm, int screen_w, int screen_h);

// Cleanup: unique_ptrs already destroy weapons; clears the vector and resets state.
void weapons_cleanup(WeaponManager *wm);

// True if the player currently holds any weapon.
bool weapons_player_has_weapon(const WeaponManager *wm);
