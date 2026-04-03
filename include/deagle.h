#pragma once

#include "weapon.h"
#include <array>

// Desert Eagle — semi-auto, 7-round magazine, hits harder than the assault rifle.
// Slower fire rate and more recoil than the AR; must click each shot (no hold-to-fire).
// Sounds and textures are loaded in the constructor and unloaded in the destructor.
class Deagle final : public Weapon {
public:
    Deagle();            // loads textures and sounds
    ~Deagle() override;  // unloads textures and sounds

    // ── Weapon interface ──────────────────────────────────────────────
    std::string_view type_name()      const override { return "deagle"; }
    int              sprite_width()   const override { return 16; }
    int              sprite_height()  const override { return 8; }
    float            render_scale()   const override { return 3.0f; }
    float            fire_rate()      const override { return 3.0f; }
    int              magazine_size()  const override { return 7; }
    float            reload_time()    const override { return 2.0f; }
    float            bullet_speed()   const override { return 900.0f; }
    int              bullet_damage()  const override { return 2; }
    float            spread_angle()   const override { return 1.5f; }  // degrees
    int              initial_reserve() const override { return 21; }   // 3 full mags
    float            recoil_distance() const override { return 7.0f; } // heavier kick
    bool             is_auto_fire()   const override { return false; } // semi-auto

    const Texture2D& texture()         const override { return texture_; }
    const Texture2D& outline_texture() const override { return outline_texture_; }

    // Fires one accurate bullet with tight spread.
    void fire(BulletSystem *bullets, Vector2 muzzle, Vector2 aim_dir) override;

    // Plays a loud, punchy shot sound with slight pitch variation.
    void play_shot_sound(float master_vol, float sfx_vol) override;

    void play_reload_sound(float master_vol, float sfx_vol) override;

private:
    Texture2D texture_{};
    Texture2D outline_texture_{};  // guns/Pistols/Outlined/Deagle.png — shown when on ground
    Sound snd_shot_{};
    Sound snd_reload_{};
    std::array<Sound, 4> shot_aliases_{};
    int shot_alias_idx_ = 0;
};
