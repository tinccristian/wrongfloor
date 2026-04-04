#include "options_menu.h"
#include "game.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

// ── Layout ────────────────────────────────────────────────────────────────────
static constexpr float BASE_FONT_SIZE   = 18.0f;
static constexpr float SEL_FONT_SIZE    = 23.4f;
static constexpr float SEL_X_OFFSET     = 12.0f;
static constexpr float ITEM_SPACING     = 38.0f;
static constexpr float LEFT_MARGIN_FRAC = 0.10f;
static constexpr float LERP_SPEED       = 9.0f;
// Number of items visible on screen at once (leaves room for header + hint)
static constexpr int   VISIBLE_ROWS     = 14;

// ── Colour ────────────────────────────────────────────────────────────────────
static constexpr unsigned char UNSEL_ALPHA   = 80;
static constexpr unsigned char SEL_ALPHA     = 255;
static constexpr unsigned char HEADER_ALPHA  = 160;

// ── Sound pitches ─────────────────────────────────────────────────────────────
static constexpr float NAV_PITCH_MIN = 0.9f;
static constexpr float NAV_PITCH_MAX = 1.1f;

static void play_nav_sound(AudioState *audio)
{
    if (!audio) return;
    StopSound(audio->snd_menu);
    SetSoundPitch(audio->snd_menu, NAV_PITCH_MIN +
        ((float)std::rand() / (float)RAND_MAX) * (NAV_PITCH_MAX - NAV_PITCH_MIN));
    SetSoundVolume(audio->snd_menu, audio->master_volume * audio->sfx_volume);
    PlaySound(audio->snd_menu);
}

// ── Helpers ───────────────────────────────────────────────────────────────────
static std::string format_volume(float vol)
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(0) << (vol * 100.0f) << "%";
    return ss.str();
}

static const char *window_mode_label(WindowMode m)
{
    switch (m)
    {
        case WindowMode::WINDOWED:    return "WINDOWED";
        case WindowMode::BORDERLESS:  return "BORDERLESS";
        case WindowMode::FULLSCREEN:  return "FULLSCREEN";
    }
    return "WINDOWED";
}

// ─────────────────────────────────────────────────────────────────────────────

static void rebuild_items(OptionsMenu *menu)
{
    const Settings *s = menu->settings;
    std::vector<OptionsItem> items;

    // DISPLAY
    items.push_back({ "DISPLAY", "", 0.f, true, 0 });
    items.push_back({ "  RESOLUTION",   RESOLUTIONS[s->res_idx].label,  0.f, false, 0 });
    items.push_back({ "  WINDOW MODE",  window_mode_label(s->window_mode), 0.f, false, 0 });
    items.push_back({ "  VSYNC",        s->vsync ? "ON" : "OFF",         0.f, false, 0 });

    // AUDIO
    items.push_back({ "", "", 0.f, false, -1 });
    items.push_back({ "AUDIO", "", 0.f, true, 1 });
    items.push_back({ "  MASTER VOLUME", format_volume(s->master_volume), 0.f, false, 1 });
    items.push_back({ "  SFX VOLUME",    format_volume(s->sfx_volume),    0.f, false, 1 });
    items.push_back({ "  MUSIC VOLUME",  format_volume(s->music_volume),  0.f, false, 1 });

    // GRAPHICS
    items.push_back({ "", "", 0.f, false, -1 });
    items.push_back({ "GRAPHICS", "", 0.f, true, 2 });
    items.push_back({ "  SHOW FPS", s->show_fps ? "ON" : "OFF", 0.f, false, 2 });

    // CONTROLS  (read-only)
    items.push_back({ "", "", 0.f, false, -1 });
    items.push_back({ "CONTROLS", "", 0.f, true, 3 });
    items.push_back({ "  MOVE",   "WASD / LEFT STICK",    0.f, false, 3 });
    items.push_back({ "  AIM",    "MOUSE / RIGHT STICK",  0.f, false, 3 });
    items.push_back({ "  SHOOT",  "LEFT CLICK / LT",      0.f, false, 3 });
    items.push_back({ "  MELEE",  "RIGHT CLICK / RT",     0.f, false, 3 });
    items.push_back({ "  RELOAD", "R / X",                0.f, false, 3 });
    items.push_back({ "  PAUSE",  "ESC / START",          0.f, false, 3 });

    int old_sel = menu->current_section.selected;
    menu->current_section.items = std::move(items);

    // Restore selection if still valid
    int total = (int)menu->current_section.items.size();
    if (old_sel >= 0 && old_sel < total &&
        !menu->current_section.items[old_sel].is_header &&
        menu->current_section.items[old_sel].section_idx >= 0)
    {
        menu->current_section.selected = old_sel;
        menu->current_section.items[old_sel].anim_t = 1.0f;
    }
}

void options_menu_init(OptionsMenu *menu, OptionsSection /*section*/, Settings *settings, AudioState *audio)
{
    menu->settings = settings;
    menu->audio = audio;
    menu->scroll_offset = 0;
    menu->current_section.selected = 0;
    menu->current_section.items.clear();

    rebuild_items(menu);

    // Advance past headers to the first real item
    int total = (int)menu->current_section.items.size();
    for (int i = 0; i < total; ++i)
    {
        if (!menu->current_section.items[i].is_header &&
            menu->current_section.items[i].section_idx >= 0)
        {
            menu->current_section.selected = i;
            menu->current_section.items[i].anim_t = 1.0f;
            break;
        }
    }
}

OptionsAction options_menu_update(OptionsMenu *menu, float dt, int screen_w, int screen_h)
{
    if (!menu->settings) return OptionsAction::None;

    int total = (int)menu->current_section.items.size();

    // ── Navigation up/down ─────────────────────────────────────────────
    int dir = 0;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) ||
        IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN))
        dir = +1;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) ||
        IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP))
        dir = -1;

    if (dir != 0)
    {
        int new_sel = menu->current_section.selected;
        for (int i = 0; i < total; ++i)
        {
            new_sel = (new_sel + dir + total) % total;
            if (!menu->current_section.items[new_sel].is_header &&
                menu->current_section.items[new_sel].section_idx >= 0)
                break;
        }
        menu->current_section.selected = new_sel;

        // Scroll to keep selected visible
        if (menu->current_section.selected < menu->scroll_offset)
            menu->scroll_offset = menu->current_section.selected;
        if (menu->current_section.selected >= menu->scroll_offset + VISIBLE_ROWS)
            menu->scroll_offset = menu->current_section.selected - VISIBLE_ROWS + 1;

        if (menu->audio) play_nav_sound(menu->audio);
    }

    // ── Value change left/right ────────────────────────────────────────
    int delta = 0;
    if (IsKeyPressed(KEY_LEFT)  || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT))
        delta = -1;
    if (IsKeyPressed(KEY_RIGHT) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT))
        delta = +1;

    if (delta != 0)
    {
        int sel = menu->current_section.selected;
        auto &item = menu->current_section.items[sel];
        Settings &s = *menu->settings;
        bool changed = false;

        if (item.label == "  RESOLUTION")
        {
            s.res_idx = (s.res_idx + delta + RESOLUTION_COUNT) % RESOLUTION_COUNT;
            item.value = RESOLUTIONS[s.res_idx].label;
            settings_apply_display(&s, nullptr, nullptr);
            changed = true;
        }
        else if (item.label == "  WINDOW MODE")
        {
            int m = ((int)s.window_mode + delta + 3) % 3;
            s.window_mode = (WindowMode)m;
            item.value = window_mode_label(s.window_mode);
            settings_apply_display(&s, nullptr, nullptr);
            changed = true;
        }
        else if (item.label == "  VSYNC")
        {
            s.vsync = s.vsync ? 0 : 1;
            item.value = s.vsync ? "ON" : "OFF";
            settings_apply_display(&s, nullptr, nullptr);
            changed = true;
        }
        else if (item.label == "  MASTER VOLUME")
        {
            s.master_volume = std::clamp(s.master_volume + delta * 0.05f, 0.0f, 1.0f);
            item.value = format_volume(s.master_volume);
            if (menu->audio) {
                audio_set_master_volume(menu->audio, s.master_volume);
                play_nav_sound(menu->audio);
            }
            changed = true;
        }
        else if (item.label == "  SFX VOLUME")
        {
            s.sfx_volume = std::clamp(s.sfx_volume + delta * 0.05f, 0.0f, 1.0f);
            item.value = format_volume(s.sfx_volume);
            if (menu->audio) {
                audio_set_sfx_volume(menu->audio, s.sfx_volume);
                play_nav_sound(menu->audio);
            }
            changed = true;
        }
        else if (item.label == "  MUSIC VOLUME")
        {
            s.music_volume = std::clamp(s.music_volume + delta * 0.05f, 0.0f, 1.0f);
            item.value = format_volume(s.music_volume);
            if (menu->audio) {
                audio_set_music_volume(menu->audio, s.music_volume);
                play_nav_sound(menu->audio);
            }
            changed = true;
        }
        else if (item.label == "  SHOW FPS")
        {
            s.show_fps = s.show_fps ? 0 : 1;
            item.value = s.show_fps ? "ON" : "OFF";
            changed = true;
        }

        if (changed && menu->audio &&
            item.label != "  MASTER VOLUME" &&
            item.label != "  SFX VOLUME" &&
            item.label != "  MUSIC VOLUME")
        {
            play_nav_sound(menu->audio);
        }
    }

    // ── Back ────────────────────────────────────────────────────────────
    if (IsKeyPressed(KEY_ESCAPE) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT))
        return OptionsAction::Back;

    // ── Lerp animations ───────────────────────────────────────────────
    for (int i = 0; i < total; ++i)
    {
        if (menu->current_section.items[i].is_header ||
            menu->current_section.items[i].section_idx < 0)
        {
            menu->current_section.items[i].anim_t = 0.0f;
        }
        else
        {
            float target = (i == menu->current_section.selected) ? 1.0f : 0.0f;
            menu->current_section.items[i].anim_t += (target - menu->current_section.items[i].anim_t) * LERP_SPEED * dt;
        }
    }

    return OptionsAction::None;
}

void options_menu_draw(const OptionsMenu *menu, int screen_w, int screen_h)
{
    DrawRectangle(0, 0, screen_w, screen_h, { 0, 0, 0, 160 });

    DrawTextEx(GetFontDefault(), "OPTIONS", { 50.0f, 30.0f }, 32.0f, 0.5f, WHITE);

    float base_x = screen_w * LEFT_MARGIN_FRAC;
    float start_y = 85.0f;
    int   total   = (int)menu->current_section.items.size();

    for (int vis = 0; vis < VISIBLE_ROWS; ++vis)
    {
        int i = vis + menu->scroll_offset;
        if (i >= total) break;

        const auto &item = menu->current_section.items[i];
        float t          = item.anim_t;
        float font_size  = BASE_FONT_SIZE + (SEL_FONT_SIZE - BASE_FONT_SIZE) * t;
        float x_offset   = SEL_X_OFFSET * t;
        float spacing    = font_size * 0.05f;
        float y          = start_y + vis * ITEM_SPACING;

        if (item.section_idx < 0) continue;  // blank row

        if (item.is_header)
        {
            DrawTextEx(GetFontDefault(), item.label.c_str(),
                       { base_x, y }, font_size + 2.0f, spacing,
                       Color{ 200, 200, 200, HEADER_ALPHA });
        }
        else
        {
            unsigned char a = (unsigned char)(UNSEL_ALPHA + (SEL_ALPHA - UNSEL_ALPHA) * t);
            Color col = { 255, 255, 255, a };

            DrawTextEx(GetFontDefault(), item.label.c_str(),
                       { base_x + x_offset, y }, font_size, spacing, col);

            if (!item.value.empty())
            {
                Vector2 vsz = MeasureTextEx(GetFontDefault(), item.value.c_str(), font_size, spacing);
                DrawTextEx(GetFontDefault(), item.value.c_str(),
                           { screen_w - 100.0f - vsz.x, y }, font_size, spacing, col);
            }
        }
    }

    // Scroll indicators
    if (menu->scroll_offset > 0)
        DrawTextEx(GetFontDefault(), "▲ scroll up",
                   { base_x, start_y - 18.0f }, 13.0f, 0.5f,
                   Color{ 200, 200, 200, 140 });
    if (menu->scroll_offset + VISIBLE_ROWS < total)
        DrawTextEx(GetFontDefault(), "▼ scroll down",
                   { base_x, start_y + VISIBLE_ROWS * ITEM_SPACING }, 13.0f, 0.5f,
                   Color{ 200, 200, 200, 140 });

    DrawTextEx(GetFontDefault(),
               "UP/DOWN: Navigate    LEFT/RIGHT: Change    ESC: Back",
               { 50.0f, (float)screen_h - 40.0f }, 14.0f, 0.5f,
               Color{ 200, 200, 200, 150 });
}
