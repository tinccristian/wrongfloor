#include "weapons/saber.h"
#include "game.h"      // assets_path()
#include <cstdlib>

static float randf_s(float lo, float hi)
{
    return lo + (hi - lo) * ((float)std::rand() / (float)RAND_MAX);
}

Saber::Saber()
{
    texture_         = LoadTexture(assets_path("guns/melee/saber.png").c_str());
    outline_texture_ = LoadTexture(assets_path("guns/melee/Outlined/saber.png").c_str());

    snd_ignition_ = LoadSound(assets_path("sounds/saber_ignition.mp3").c_str());
    snd_attack_   = LoadSound(assets_path("sounds/saber_attack.mp3").c_str());
    snd_humming_  = LoadMusicStream(assets_path("sounds/saber_humming.mp3").c_str());
    snd_humming_.looping = true;
}

Saber::~Saber()
{
    if (humming_playing_)
    {
        StopMusicStream(snd_humming_);
        humming_playing_ = false;
    }
    if (snd_humming_.ctxData)        UnloadMusicStream(snd_humming_);
    if (snd_attack_.stream.buffer)   UnloadSound(snd_attack_);
    if (snd_ignition_.stream.buffer) UnloadSound(snd_ignition_);
    if (outline_texture_.id != 0)    UnloadTexture(outline_texture_);
    if (texture_.id != 0)            UnloadTexture(texture_);
}

void Saber::play_shot_sound(float master_vol, float sfx_vol)
{
    SetSoundPitch(snd_attack_, randf_s(0.95f, 1.05f));
    SetSoundVolume(snd_attack_, master_vol * sfx_vol);
    PlaySound(snd_attack_);
}

void Saber::update_held(float /*dt*/, float master_vol, float sfx_vol)
{
    if (!humming_playing_) return;
    SetMusicVolume(snd_humming_, master_vol * sfx_vol * 0.6f);
    UpdateMusicStream(snd_humming_);
}

void Saber::on_pickup(float master_vol, float sfx_vol)
{
    SetSoundVolume(snd_ignition_, master_vol * sfx_vol);
    PlaySound(snd_ignition_);

    if (!humming_playing_)
    {
        SetMusicVolume(snd_humming_, master_vol * sfx_vol * 0.6f);
        PlayMusicStream(snd_humming_);
        humming_playing_ = true;
    }
}

void Saber::on_dropped()
{
    if (humming_playing_)
    {
        StopMusicStream(snd_humming_);
        humming_playing_ = false;
    }
}
