#include "pause_menu.h"
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
static const Color OVERLAY_COLOR = { 0, 0, 0, 160 };

// ── Lerp speed ────────────────────────────────────────────────────────────────
static constexpr float LERP_SPEED = 9.0f;

// ── Sound pitches ─────────────────────────────────────────────────────────────
static constexpr float NAV_PITCH_MIN  = 0.9f;
static constexpr float NAV_PITCH_MAX  = 1.1f;
static constexpr float CONF_PITCH_MIN = 0.7f;
static constexpr float CONF_PITCH_MAX = 0.8f;
static constexpr float CONF_VOL_MULT  = 1.2f;

// ── Shared layout helper ──────────────────────────────────────────────────────
// Returns the draw rect for item i given its current anim_t.
// x/y is the top-left of the text; width/height matches the rendered glyph area.
static Rectangle item_rect(const PauseMenu *menu, int i, int screen_w, int screen_h)
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

void pause_menu_init(PauseMenu *menu)
{
    menu->items.clear();
    menu->selected = 0;

    for (const char *label : { "RESUME", "RESTART", "MAIN MENU", "EXIT" })
        menu->items.push_back({ label, 0.0f });

    // Pre-select the first item without animation.
    menu->items[0].anim_t = 1.0f;
}

PauseAction pause_menu_update(PauseMenu *menu, AudioState *audio, float dt, int screen_w, int screen_h)
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
    Vector2 mouse = GetMousePosition();
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
            case 0: return PauseAction::Resume;
            case 1: return PauseAction::Restart;
            case 2: return PauseAction::MainMenu;
            case 3: return PauseAction::Exit;
            default: break;
        }
    }

    // ── Lerp animations ───────────────────────────────────────────────
    for (int i = 0; i < count; ++i)
    {
        float target = (i == menu->selected) ? 1.0f : 0.0f;
        float &t = menu->items[i].anim_t;
        t += (target - t) * LERP_SPEED * dt;
        t = std::clamp(t, 0.0f, 1.0f);
    }

    return PauseAction::None;
}

void pause_menu_draw(const PauseMenu *menu, int screen_w, int screen_h)
{
    DrawRectangle(0, 0, screen_w, screen_h, OVERLAY_COLOR);

    Font font = GetFontDefault();

    int count = (int)menu->items.size();
    for (int i = 0; i < count; ++i)
    {
        const PauseMenuItem &item = menu->items[i];
        float t = item.anim_t;

        float font_size = BASE_FONT_SIZE + (SEL_FONT_SIZE - BASE_FONT_SIZE) * t;
        float spacing   = font_size * 0.05f;
        unsigned char alpha = (unsigned char)(UNSEL_ALPHA + (SEL_ALPHA - UNSEL_ALPHA) * t);
        Color col = { 255, 255, 255, alpha };

        Rectangle r = item_rect(menu, i, screen_w, screen_h);

        // Shadow
        DrawTextEx(font, item.label.c_str(),
                   { r.x + 2.0f, r.y + 2.0f }, font_size, spacing,
                   Color{ 0, 0, 0, (unsigned char)(alpha / 2) });

        // Main text
        DrawTextEx(font, item.label.c_str(),
                   { r.x, r.y }, font_size, spacing, col);
    }
}
