#pragma once

#include "weapon.h"
#include <array>

// Assault rifle — full-auto, 30-round magazine, single projectile per shot.
// All stats are compile-time constants; sounds and textures are loaded in the
// constructor and unloaded in the destructor (RAII).
class AssaultRifle final : public Weapon {
public:
    AssaultRifle();            // loads textures and sounds
    ~AssaultRifle() override;  // unloads textures and sounds

    // ── Weapon interface ──────────────────────────────────────────────
    std::string_view type_name()      const override { return "assault_riffle"; }
    int              sprite_width()   const override { return 35; }
    int              sprite_height()  const override { return 11; }
    float            render_scale()   const override { return 2.0f; }
    float            fire_rate()      const override { return 10.0f; }
    int              magazine_size()  const override { return 30; }
    float            reload_time()    const override { return 2.8f; }
    float            bullet_speed()   const override { return 900.0f; }
    int              bullet_damage()  const override { return 1; }
    float            spread_angle()   const override { return 4.0f; }  // degrees
    int              initial_reserve() const override { return 90; }   // 3 full mags
    bool             is_auto_fire()   const override { return true; }

    const Texture2D& texture()         const override { return texture_; }
    const Texture2D& outline_texture() const override { return outline_texture_; }

    // Fires one bullet with randomised spread within spread_angle().
    void fire(BulletSystem *bullets, Vector2 muzzle, Vector2 aim_dir,
              BulletOwner owner = BulletOwner::PLAYER) override;

    // Plays one aliased shot sound with random pitch for natural-sounding rapid fire.
    void play_shot_sound(float master_vol, float sfx_vol) override;

    void play_reload_sound(float master_vol, float sfx_vol) override;

private:
    Texture2D texture_{};
    Texture2D outline_texture_{};
    Sound snd_shot_{};
    Sound snd_reload_{};
    std::array<Sound, 4> shot_aliases_{};
    int shot_alias_idx_ = 0;
};
