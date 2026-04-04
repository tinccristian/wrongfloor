#include "deagle.h"
#include "game.h"      // assets_path()
#include "raymath.h"
#include <cmath>
#include <cstdlib>

// ── Local helpers ─────────────────────────────────────────────────────────────

static float randf(float lo, float hi)
{
    return lo + (hi - lo) * ((float)std::rand() / (float)RAND_MAX);
}

// ── Deagle ────────────────────────────────────────────────────────────────────

Deagle::Deagle()
{
    texture_         = LoadTexture(assets_path("guns/Pistols/deagle.png").c_str());
    outline_texture_ = LoadTexture(assets_path("guns/Pistols/Outlined/Deagle.png").c_str());
    snd_shot_        = LoadSound(assets_path("sounds/deagle_shot.mp3").c_str());
    snd_reload_ = LoadSound(assets_path("sounds/deagle_reload.mp3").c_str());
    for (int i = 0; i < 4; ++i)
        shot_aliases_[i] = LoadSoundAlias(snd_shot_);
}

Deagle::~Deagle()
{
    // Unload aliases before the source sound — raylib requires this order.
    for (const auto& alias : shot_aliases_)
        if (alias.stream.buffer) UnloadSoundAlias(alias);
    if (snd_shot_.stream.buffer)   UnloadSound(snd_shot_);
    if (snd_reload_.stream.buffer) UnloadSound(snd_reload_);
    if (texture_.id != 0)         UnloadTexture(texture_);
    if (outline_texture_.id != 0) UnloadTexture(outline_texture_);
}

void Deagle::fire(BulletSystem *bullets, Vector2 muzzle, Vector2 aim_dir,
                  BulletOwner owner)
{
    float base   = atan2f(aim_dir.y, aim_dir.x);
    float spread = randf(-spread_angle(), spread_angle()) * DEG2RAD;
    float ang    = base + spread;
    Vector2 dir  = { cosf(ang), sinf(ang) };
    bullets_spawn(bullets, muzzle, dir, bullet_speed(), owner);
}

void Deagle::play_shot_sound(float master_vol, float sfx_vol)
{
    Sound& alias = shot_aliases_[shot_alias_idx_];
    shot_alias_idx_ = (shot_alias_idx_ + 1) % 4;
    SetSoundPitch(alias, randf(0.9f, 1.1f));
    // 1.4x louder than base volume for a punchy feel.
    SetSoundVolume(alias, master_vol * sfx_vol * 1.4f);
    PlaySound(alias);
}

void Deagle::play_reload_sound(float master_vol, float sfx_vol)
{
    SetSoundVolume(snd_reload_, master_vol * sfx_vol);
    PlaySound(snd_reload_);
}
