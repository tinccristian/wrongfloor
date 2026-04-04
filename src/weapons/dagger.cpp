#include "weapons/dagger.h"
#include "game.h"      // assets_path()
#include "raymath.h"
#include <cmath>
#include <cstdlib>

// ── Helpers ───────────────────────────────────────────────────────────────────

static float randf_d(float lo, float hi)
{
    return lo + (hi - lo) * ((float)std::rand() / (float)RAND_MAX);
}

// Same dark palette as effects.cpp settle colors.
static const Color BLOOD_COLORS[] = {
    { 120,  0,  0, 255 },
    { 100,  0,  0, 255 },
    {  80,  0,  0, 255 },
    {  50,  0,  0, 255 },
    { 140, 20,  0, 255 },
};
static constexpr int BLOOD_COLOR_COUNT = 5;

static Color random_blood_color()
{
    return BLOOD_COLORS[std::rand() % BLOOD_COLOR_COUNT];
}

// ── Dagger ────────────────────────────────────────────────────────────────────

Dagger::Dagger()
{
    texture_         = LoadTexture(assets_path("guns/melee/dagger.png").c_str());
    outline_texture_ = LoadTexture(assets_path("guns/melee/Outlined/dagger.png").c_str());

    snd_draw_  = LoadSound(assets_path("sounds/dagger_draw.mp3").c_str());
    snd_flesh_ = LoadSound(assets_path("sounds/dagger_flesh.mp3").c_str());
    snd_attack_ = LoadSound(assets_path("sounds/dagger_attack.mp3").c_str());
    for (int i = 0; i < 4; ++i)
        attack_aliases_[i] = LoadSoundAlias(snd_attack_);
}

Dagger::~Dagger()
{
    for (const auto& a : attack_aliases_)
        if (a.stream.buffer) UnloadSoundAlias(a);
    if (snd_attack_.stream.buffer)   UnloadSound(snd_attack_);
    if (snd_flesh_.stream.buffer)    UnloadSound(snd_flesh_);
    if (snd_draw_.stream.buffer)     UnloadSound(snd_draw_);
    if (outline_texture_.id != 0)    UnloadTexture(outline_texture_);
    if (texture_.id != 0)            UnloadTexture(texture_);
}

void Dagger::play_shot_sound(float master_vol, float sfx_vol)
{
    Sound& alias = attack_aliases_[attack_alias_idx_];
    attack_alias_idx_ = (attack_alias_idx_ + 1) % 4;
    SetSoundPitch(alias, randf_d(0.85f, 1.15f));
    SetSoundVolume(alias, master_vol * sfx_vol);
    PlaySound(alias);
}

void Dagger::on_pickup(float master_vol, float sfx_vol)
{
    SetSoundVolume(snd_draw_, master_vol * sfx_vol);
    PlaySound(snd_draw_);
}

void Dagger::on_dropped()
{
    blood_pixels_.clear();
}

void Dagger::on_melee_hit(int hit_count, float master_vol, float sfx_vol)
{
    // Play flesh impact sound.
    SetSoundPitch(snd_flesh_, randf_d(0.9f, 1.1f));
    SetSoundVolume(snd_flesh_, master_vol * sfx_vol);
    PlaySound(snd_flesh_);

    // Add blood decals to the blade: 3-5 pixels per hit, capped at MAX_BLOOD.
    float blade_len = (float)sprite_width() * render_scale();   // px along blade
    float half_h    = (float)sprite_height() * render_scale() * 0.5f;

    int to_add = hit_count * (3 + std::rand() % 3); // 3-5 per hit
    for (int i = 0; i < to_add && (int)blood_pixels_.size() < MAX_BLOOD; ++i)
    {
        BloodPixel bp;
        bp.x     = randf_d(blade_len * 0.3f, blade_len);  // avoid near-grip, bias toward tip
        bp.y     = randf_d(-half_h, half_h);
        bp.color = random_blood_color();
        blood_pixels_.push_back(bp);
    }
}

void Dagger::draw_overlay(Vector2 render_pos, float render_rotation) const
{
    if (blood_pixels_.empty()) return;

    float rad  = render_rotation * DEG2RAD;
    Vector2 along = { cosf(rad),  sinf(rad) };
    Vector2 perp  = { -sinf(rad), cosf(rad) };

    for (const auto& bp : blood_pixels_)
    {
        Vector2 wp = {
            render_pos.x + along.x * bp.x + perp.x * bp.y,
            render_pos.y + along.y * bp.x + perp.y * bp.y
        };
        DrawRectangle((int)wp.x, (int)wp.y, 2, 2, bp.color);
    }
}
