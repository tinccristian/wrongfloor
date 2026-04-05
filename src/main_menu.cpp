#include "main_menu.h"
#include "game.h"
#include <algorithm>
#include <cmath>

// ── Layout ────────────────────────────────────────────────────────────────────
static constexpr float BASE_FONT_SIZE   = 24.0f;
static constexpr float SEL_FONT_SIZE    = 31.2f;   // 1.3× base
static constexpr float SEL_X_OFFSET     = 20.0f;   // px nudge right when selected
static constexpr float ITEM_SPACING     = 52.0f;   // px between item baselines
static constexpr float LEFT_MARGIN_FRAC = 0.37f;   // base x = screen_w * this

// ── Colour ────────────────────────────────────────────────────────────────────
static constexpr unsigned char UNSEL_ALPHA = 100;
static constexpr unsigned char SEL_ALPHA   = 255;

// ── Lerp speed ────────────────────────────────────────────────────────────────
static constexpr float LERP_SPEED = 9.0f;

// ── Sound pitches ─────────────────────────────────────────────────────────────
static constexpr float NAV_PITCH_MIN  = 0.9f;
static constexpr float NAV_PITCH_MAX  = 1.1f;
static constexpr float CONF_PITCH_MIN = 0.7f;
static constexpr float CONF_PITCH_MAX = 0.8f;
static constexpr float CONF_VOL_MULT  = 1.2f;

// ── Shared layout helper ──────────────────────────────────────────────────────
static Rectangle item_rect(const MainMenu *menu, int i, int screen_w, int screen_h)
{
    int   count   = (int)menu->items.size();
    float total_h = BASE_FONT_SIZE + (count - 1) * ITEM_SPACING;
    float start_y = (screen_h - total_h) * 0.5f;
    float base_x  = screen_w * LEFT_MARGIN_FRAC;

    float t         = menu->items[i].anim_t;
    float font_size = BASE_FONT_SIZE + (SEL_FONT_SIZE - BASE_FONT_SIZE) * t;
    float x_offset  = SEL_X_OFFSET * t;
    float spacing   = font_size * 0.05f;

    Font    font    = GetFontDefault();
    Vector2 text_sz = MeasureTextEx(font, menu->items[i].label.c_str(), font_size, spacing);

    return { base_x + x_offset, start_y + i * ITEM_SPACING, text_sz.x, font_size };
}

// ── Sound helpers ─────────────────────────────────────────────────────────────
static void play_nav_sound(AudioState *audio)
{
    StopSound(audio->snd_menu);
    SetSoundPitch(audio->snd_menu, NAV_PITCH_MIN +
        ((float)std::rand() / (float)RAND_MAX) * (NAV_PITCH_MAX - NAV_PITCH_MIN));
    SetSoundVolume(audio->snd_menu, audio->master_volume * audio->sfx_volume);
    PlaySound(audio->snd_menu);
}

static void play_confirm_sound(AudioState *audio)
{
    StopSound(audio->snd_menu);
    SetSoundPitch(audio->snd_menu, CONF_PITCH_MIN +
        ((float)std::rand() / (float)RAND_MAX) * (CONF_PITCH_MAX - CONF_PITCH_MIN));
    float vol = std::min(audio->master_volume * audio->sfx_volume * CONF_VOL_MULT, 1.0f);
    SetSoundVolume(audio->snd_menu, vol);
    PlaySound(audio->snd_menu);
}

// ─────────────────────────────────────────────────────────────────────────────

void main_menu_init(MainMenu *menu)
{
    menu->items.clear();
    menu->selected = 0;

    for (const char *label : { "PLAY", "OPTIONS", "QUIT" })
        menu->items.push_back({ label, 0.0f });

    // Pre-select the first item
    menu->items[0].anim_t = 1.0f;

    // Load splash background
    if (menu->splash.id == 0)
        menu->splash = LoadTexture(assets_path("splash.png").c_str());
}

MainMenuAction main_menu_update(MainMenu *menu, AudioState *audio, float dt, int screen_w, int screen_h, Vector2 virtual_mouse)
{
    int count = (int)menu->items.size();

    // ── Keyboard / gamepad navigation ─────────────────────────────────
    int dir = 0;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) ||
        IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN))
        dir = +1;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) ||
        IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP))
        dir = -1;

    if (dir != 0)
    {
        menu->selected = (menu->selected + dir + count) % count;
        play_nav_sound(audio);
    }

    // ── Mouse hover ───────────────────────────────────────────────────
    Vector2 mouse = virtual_mouse;
    for (int i = 0; i < count; ++i)
    {
        if (CheckCollisionPointRec(mouse, item_rect(menu, i, screen_w, screen_h)))
        {
            if (menu->selected != i)
            {
                menu->selected = i;
                play_nav_sound(audio);
            }
            break;
        }
    }

    // ── Keyboard / gamepad confirm ────────────────────────────────────
    bool confirmed = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
                     IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);

    // ── Mouse click confirm ───────────────────────────────────────────
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        for (int i = 0; i < count; ++i)
        {
            if (CheckCollisionPointRec(mouse, item_rect(menu, i, screen_w, screen_h)))
            {
                menu->selected = i;
                confirmed = true;
                break;
            }
        }
    }

    if (confirmed)
    {
        play_confirm_sound(audio);
        switch (menu->selected)
        {
            case 0: return MainMenuAction::Play;
            case 1: return MainMenuAction::Options;
            case 2: return MainMenuAction::Quit;
            default: break;
        }
    }

    // ── Lerp animations ───────────────────────────────────────────────
    for (int i = 0; i < count; ++i)
    {
        float target = (i == menu->selected) ? 1.0f : 0.0f;
        menu->items[i].anim_t += (target - menu->items[i].anim_t) * LERP_SPEED * dt;
    }

    return MainMenuAction::None;
}

void main_menu_draw(const MainMenu *menu, int screen_w, int screen_h)
{
    // Draw splash background scaled to fill screen
    float scale_x = (float)screen_w / (float)menu->splash.width;
    float scale_y = (float)screen_h / (float)menu->splash.height;
    float scale = std::max(scale_x, scale_y);

    float w = menu->splash.width * scale;
    float h = menu->splash.height * scale;
    float x = (screen_w - w) * 0.5f;
    float y = (screen_h - h) * 0.5f;

    DrawTextureEx(menu->splash, { x, y }, 0.0f, scale, WHITE);

    // Draw semi-transparent overlay
    DrawRectangle(0, 0, screen_w, screen_h, { 0, 0, 0, 100 });

    // Draw menu items
    int count = (int)menu->items.size();
    for (int i = 0; i < count; ++i)
    {
        float t = menu->items[i].anim_t;
        float font_size = BASE_FONT_SIZE + (SEL_FONT_SIZE - BASE_FONT_SIZE) * t;
        float alpha = (unsigned char)(UNSEL_ALPHA + (SEL_ALPHA - UNSEL_ALPHA) * t);
        float x_offset = SEL_X_OFFSET * t;
        float spacing = font_size * 0.05f;

        Rectangle rect = item_rect(menu, i, screen_w, screen_h);
        Color color = Color{ 255, 255, 255, (unsigned char)alpha };

        DrawTextEx(GetFontDefault(), menu->items[i].label.c_str(),
                   { rect.x, rect.y }, font_size, spacing, color);
    }
}

void main_menu_cleanup(MainMenu *menu)
{
    if (menu->splash.id != 0)
        UnloadTexture(menu->splash);
    menu->splash = {};
}
