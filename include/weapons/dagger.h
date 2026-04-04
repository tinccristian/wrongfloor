#pragma once

#include "weapon.h"
#include "raylib.h"
#include <array>
#include <vector>

// Fast-stabbing melee weapon. No ammo.  Jabs forward ~10 px per attack at up to
// 7 attacks/second (hold to spam). Accumulates blood pixel decals on the blade
// with each kill; they rotate with the weapon and clear on drop.
class Dagger final : public Weapon {
public:
    Dagger();
    ~Dagger() override;

    // ── Weapon interface ──────────────────────────────────────────────
    std::string_view type_name()       const override { return "dagger"; }
    int              sprite_width()    const override { return 16; }
    int              sprite_height()   const override { return 7; }
    float            render_scale()    const override { return 3.0f; }
    float            fire_rate()       const override { return 7.0f; }   // stabs/sec
    int              magazine_size()   const override { return 0; }
    float            reload_time()     const override { return 0.0f; }
    float            bullet_speed()    const override { return 0.0f; }
    int              bullet_damage()   const override { return 1; }
    float            spread_angle()    const override { return 0.0f; }
    int              initial_reserve() const override { return 0; }
    bool             is_auto_fire()    const override { return false; }  // press per stab, not hold

    bool  is_melee()         const override { return true; }
    float melee_range()      const override { return 70.0f; }  // 2.5x, starts past the player
    float melee_width()      const override { return 50.0f; }  // 2.5x
    float swing_duration()   const override { return 0.10f; }  // seconds
    float swing_peak_fwd()   const override { return 10.0f; }  // px jab distance

    const Texture2D& texture()         const override { return texture_; }
    const Texture2D& outline_texture() const override { return outline_texture_; } // zero-id → runtime fallback

    void fire(BulletSystem *, Vector2, Vector2, BulletOwner) override {} // no-op
    void play_shot_sound(float master_vol, float sfx_vol) override;
    void play_reload_sound(float, float) override {}

    void on_pickup(float master_vol, float sfx_vol) override;
    void on_dropped() override;
    void on_melee_hit(int hit_count, float master_vol, float sfx_vol) override;
    void draw_overlay(Vector2 render_pos, float render_rotation) const override;

private:
    static constexpr int MAX_BLOOD = 25;

    Texture2D texture_{};
    Texture2D outline_texture_{};  // intentionally empty — runtime outline fallback

    Sound snd_draw_{};
    Sound snd_flesh_{};
    Sound snd_attack_{};
    std::array<Sound, 4> attack_aliases_{};
    int attack_alias_idx_ = 0;

    // Blood decals in sprite-local space.
    // x = distance along blade from grip (world px), y = perpendicular offset (world px).
    struct BloodPixel { float x; float y; Color color; };
    std::vector<BloodPixel> blood_pixels_;
};
