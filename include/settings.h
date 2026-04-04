#pragma once

#include <string>

// Window mode: 0 = windowed, 1 = borderless, 2 = fullscreen
enum class WindowMode { WINDOWED = 0, BORDERLESS = 1, FULLSCREEN = 2 };

// Preset resolutions
struct Resolution { int w; int h; const char *label; };
static const Resolution RESOLUTIONS[] = {
    { 1280,  720, "1280x720"  },
    { 1600,  900, "1600x900"  },
    { 1920, 1080, "1920x1080" },
    { 2560, 1440, "2560x1440" },
};
static constexpr int RESOLUTION_COUNT = 4;

struct Settings {
    // Display
    int        res_idx     = 0;                       // index into RESOLUTIONS[]
    WindowMode window_mode = WindowMode::WINDOWED;
    int        vsync       = 1;

    // Audio
    float master_volume = 0.8f;
    float sfx_volume    = 0.8f;
    float music_volume  = 0.8f;

    // Graphics
    int show_fps = 0;
};

// Load settings from %APPDATA%\wrongfloor\settings.json
void settings_load(Settings *settings);

// Save settings to the config directory
void settings_save(const Settings *settings);

// Apply all display settings to the current window (safe to call after InitWindow).
// Updates *screen_w/*screen_h to the new window dimensions.
void settings_apply_display(const Settings *settings, int *screen_w, int *screen_h);
