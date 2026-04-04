#pragma once

#include "weapon.h"

// Lightsaber-style melee weapon. Swings through a wide 80° arc in ~0.3 s.
// No ammo or reload. Hums continuously while held via a looping music stream;
// plays an ignition sound on pickup and stops humming immediately when dropped.
class Saber final : public Weapon {
public:
    Saber();
    ~Saber() override;

    // ── Weapon interface ──────────────────────────────────────────────
    std::string_view type_name()       const override { return "saber"; }
    int              sprite_width()    const override { return 34; }
    int              sprite_height()   const override { return 8; }
    float            render_scale()    const override { return 3.0f; }
    float            fire_rate()       const override { return 2.0f; }   // swings/sec
    int              magazine_size()   const override { return 0; }      // no ammo
    float            reload_time()     const override { return 0.0f; }
    float            bullet_speed()    const override { return 0.0f; }
    int              bullet_damage()   const override { return 1; }
    float            spread_angle()    const override { return 0.0f; }
    int              initial_reserve() const override { return 0; }
    bool             is_auto_fire()    const override { return false; }  // press per swing

    bool  is_melee()         const override { return true; }
    float melee_range()      const override { return 95.0f; }  // 2.5x original, starts ahead of orbit
    float melee_width()      const override { return 100.0f; } // 2.5x, wide arc
    float swing_duration()   const override { return 0.30f; }  // seconds
    float swing_peak_angle() const override { return 80.0f; }  // degrees arc

    const Texture2D& texture()         const override { return texture_; }
    const Texture2D& outline_texture() const override { return outline_texture_; } // zero-id → runtime fallback

    void fire(BulletSystem *, Vector2, Vector2, BulletOwner) override {} // no-op
    void play_shot_sound(float master_vol, float sfx_vol) override;
    void play_reload_sound(float, float) override {}

    void update_held(float dt, float master_vol, float sfx_vol) override;
    void on_pickup(float master_vol, float sfx_vol) override;
    void on_dropped() override;

private:
    Texture2D texture_{};
    Texture2D outline_texture_{};  // intentionally empty — runtime outline fallback

    Sound snd_ignition_{};
    Sound snd_attack_{};
    Music snd_humming_{};
    bool  humming_playing_ = false;
};
