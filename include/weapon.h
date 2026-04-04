/*
 * Weapon system architecture
 * ──────────────────────────
 * Three-layer design:
 *
 *   weapon.h / weapon.cpp
 *     Abstract Weapon base class. Defines the pure-virtual interface that every
 *     gun must implement (stats, textures, fire pattern, sounds) plus the shared
 *     runtime state (position, ammo, timers, animation) that the manager drives
 *     identically for all weapon types. No weapon-specific values live here.
 *
 *   assault_rifle.h / assault_rifle.cpp  (and future pistol.h, shotgun.h, …)
 *     Concrete subclass. Overrides every pure virtual with its own constants.
 *     Owns and manages its own texture/sound assets via RAII (load in ctor,
 *     unload in dtor). Implements fire() with its own spread/projectile logic.
 *
 *   weapon_manager.h / weapon_manager.cpp
 *     Owns all live weapon instances as std::vector<std::unique_ptr<Weapon>>.
 *     Contains all shared update logic: orbit, flip, ground-bob, throw physics,
 *     pickup, reload bar, ammo HUD, recoil — written once, driven by the base
 *     class interface, applied to every weapon type automatically.
 *     Hosts the factory function: adding a new gun = one extra else-if.
 *
 * Adding a new weapon type (e.g. Pistol):
 *   1. Create include/pistol.h + src/pistol.cpp — copy assault_rifle as template.
 *   2. Override all pure virtuals with pistol values.
 *   3. Add an else-if for "pistol" in weapons_spawn() in weapon_manager.cpp.
 *   4. Place Tiled objects with weapon_type="pistol". Done — no other files change.
 */
#pragma once

#include "raylib.h"
#include "bullets.h"
#include <string_view>

// Abstract base class for all weapon types.
// Subclasses override the pure-virtual interface; the manager drives shared behavior
// through this interface without knowing which concrete type it holds.
class Weapon {
public:
    virtual ~Weapon() = default;

    // Non-copyable — owned exclusively by WeaponManager via unique_ptr.
    Weapon(const Weapon&)            = delete;
    Weapon& operator=(const Weapon&) = delete;

    // ── Weapon-specific interface (implement in each subclass) ────────
    virtual std::string_view type_name()      const = 0;
    virtual int              sprite_width()   const = 0;
    virtual int              sprite_height()  const = 0;
    virtual float            render_scale()   const = 0;
    virtual float            fire_rate()      const = 0;   // shots per second
    virtual int              magazine_size()  const = 0;
    virtual float            reload_time()    const = 0;   // seconds
    virtual float            bullet_speed()   const = 0;   // px/sec
    virtual int              bullet_damage()  const = 0;
    virtual float            spread_angle()   const = 0;   // degrees, half-width
    virtual bool             is_auto_fire()   const = 0;   // hold vs press to fire

    // Starting reserve pool (ammo outside the magazine). Set once when spawned;
    // depleted by reloads. Weapon cannot reload when reserve reaches 0.
    virtual int initial_reserve() const = 0;

    // Visual recoil kick in px per shot. Override for heavier-feeling guns.
    virtual float recoil_distance() const { return 4.0f; }

    // Defaults that most weapons share; override if needed.
    virtual int  projectile_count() const { return 1; }    // >1 for shotguns
    virtual bool is_throwable()     const { return true; }

    // ── Melee interface — return false/0 for all ranged weapons ──────
    virtual bool  is_melee()         const { return false; }
    virtual float melee_range()      const { return 0.0f; }  // hitbox reach in px
    virtual float melee_width()      const { return 0.0f; }  // hitbox width in px
    virtual float swing_duration()   const { return 0.0f; }  // seconds, full cycle
    virtual float swing_peak_angle() const { return 0.0f; }  // degrees arc (saber)
    virtual float swing_peak_fwd()   const { return 0.0f; }  // px stab offset (dagger)

    // ── Lifecycle hooks called by WeaponManager ───────────────────────
    // Called every frame while held (e.g. streaming audio update).
    virtual void update_held(float /*dt*/, float /*master_vol*/, float /*sfx_vol*/) {}
    // Called once when the player picks up this weapon.
    virtual void on_pickup(float /*master_vol*/, float /*sfx_vol*/) {}
    // Called once when the weapon is dropped or thrown.
    virtual void on_dropped() {}
    // Called by the manager after melee collision reports >=1 hit.
    virtual void on_melee_hit(int /*hit_count*/, float /*master_vol*/, float /*sfx_vol*/) {}
    // Draw extra overlays after the main sprite (e.g. dagger blood pixels).
    // render_rotation already includes swing_rotation_offset.
    virtual void draw_overlay(Vector2 /*render_pos*/, float /*render_rotation*/) const {}

    // Asset access — textures owned by the subclass.
    virtual const Texture2D& texture()         const = 0;
    virtual const Texture2D& outline_texture() const = 0;

    // Spawn all projectiles for one shot at the given muzzle position.
    // Called by the manager after fire input is confirmed; muzzle is pre-calculated.
    // owner is passed through to bullets_spawn for collision filtering.
    // Melee weapons override this as a no-op.
    virtual void fire(BulletSystem *bullets, Vector2 muzzle, Vector2 aim_dir,
                      BulletOwner owner = BulletOwner::PLAYER) = 0;

    // Play the shot / attack sound. Melee: plays swing sound.
    virtual void play_shot_sound(float master_vol, float sfx_vol) = 0;

    // Play the reload-start sound. Melee: no-op.
    virtual void play_reload_sound(float master_vol, float sfx_vol) = 0;

    // ── Shared runtime state (driven by WeaponManager) ────────────────
    Vector2 position{};             // world-space center (when not held)
    int     current_ammo  = 0;     // rounds in magazine
    int     reserve_ammo  = 0;     // rounds outside magazine; depleted by reloads

    bool  is_held      = false;
    bool  is_on_ground = true;
    bool  is_thrown    = false;
    bool  is_reloading = false;
    float reload_timer        = 0.0f;
    float fire_cooldown_timer = 0.0f;

    Vector2 throw_velocity{};       // active while is_thrown

    // Visual / animation state
    float   bob_timer       = 0.0f; // sine phase for ground bobbing
    float   recoil_offset   = 0.0f; // px backward kick, lerps to 0
    float   render_rotation = 0.0f; // degrees, lerped toward aim angle
    Vector2 render_pos{};           // computed each frame when held

    // Melee swing animation state (driven by WeaponManager)
    bool  is_swinging           = false;
    float swing_timer           = 0.0f;
    float swing_rotation_offset = 0.0f; // degrees added to render_rotation during swing
    float melee_forward_offset  = 0.0f; // px forward push during stab swing
    bool  melee_hit_triggered   = false; // prevents multiple hit checks per swing

protected:
    Weapon() = default;
};
