#include "settings.h"
#include "raylib.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdlib>
#include <filesystem>

using json = nlohmann::json;

static std::string get_config_dir()
{
    // Try %APPDATA%/wrongfloor (Windows) or ~/.wrongfloor (Unix)
#ifdef _WIN32
    const char *appdata = std::getenv("APPDATA");
    if (appdata)
    {
        std::filesystem::path dir = std::filesystem::path(appdata) / "wrongfloor";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (!ec) return dir.string();
    }
#else
    const char *home = std::getenv("HOME");
    if (home)
    {
        std::filesystem::path dir = std::filesystem::path(home) / ".wrongfloor";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (!ec) return dir.string();
    }
#endif
    // Fallback: save next to the executable
    return ".";
}

static std::string settings_path()
{
    return (std::filesystem::path(get_config_dir()) / "settings.json").string();
}

void settings_load(Settings *settings)
{
    std::string path = settings_path();
    std::ifstream file(path);

    if (!file.is_open())
    {
        settings_save(settings);
        return;
    }

    try {
        json j;
        file >> j;

        if (j.contains("res_idx"))
        {
            settings->res_idx = std::clamp((int)j["res_idx"], 0, RESOLUTION_COUNT - 1);
        }
        if (j.contains("window_mode"))
        {
            int m = j["window_mode"];
            settings->window_mode = (WindowMode)std::clamp(m, 0, 2);
        }
        if (j.contains("vsync"))      settings->vsync        = j["vsync"];
        if (j.contains("master_vol")) settings->master_volume= j["master_vol"];
        if (j.contains("sfx_vol"))    settings->sfx_volume   = j["sfx_vol"];
        if (j.contains("music_vol"))  settings->music_volume = j["music_vol"];
        if (j.contains("show_fps"))   settings->show_fps     = j["show_fps"];
    } catch (...) {
        // Bad JSON — leave defaults in place
    }
}

void settings_save(const Settings *settings)
{
    std::string path = settings_path();
    TraceLog(LOG_INFO, "SETTINGS: saving to %s", path.c_str());
    std::ofstream file(path);
    if (!file.is_open())
    {
        TraceLog(LOG_WARNING, "SETTINGS: failed to open %s for writing", path.c_str());
        return;
    }

    json j;
    j["res_idx"]     = settings->res_idx;
    j["window_mode"] = (int)settings->window_mode;
    j["vsync"]       = settings->vsync;
    j["master_vol"]  = settings->master_volume;
    j["sfx_vol"]     = settings->sfx_volume;
    j["music_vol"]   = settings->music_volume;
    j["show_fps"]    = settings->show_fps;
    file << j.dump(4) << "\n";
}

void settings_apply_display(const Settings *settings, int *screen_w, int *screen_h)
{
    const Resolution &res = RESOLUTIONS[settings->res_idx];

    // Apply window mode
    switch (settings->window_mode)
    {
        case WindowMode::WINDOWED:
            // Clear fullscreen and decoration flags
            if (IsWindowFullscreen()) ToggleFullscreen();
            ClearWindowState(FLAG_WINDOW_UNDECORATED);
            SetWindowSize(res.w, res.h);
            // Centre the window
            SetWindowPosition(
                (GetMonitorWidth(GetCurrentMonitor()) - res.w) / 2,
                (GetMonitorHeight(GetCurrentMonitor()) - res.h) / 2
            );
            break;

        case WindowMode::BORDERLESS:
            if (IsWindowFullscreen()) ToggleFullscreen();
            SetWindowSize(res.w, res.h);
            SetWindowState(FLAG_WINDOW_UNDECORATED);
            SetWindowPosition(
                (GetMonitorWidth(GetCurrentMonitor()) - res.w) / 2,
                (GetMonitorHeight(GetCurrentMonitor()) - res.h) / 2
            );
            break;

        case WindowMode::FULLSCREEN:
            ClearWindowState(FLAG_WINDOW_UNDECORATED);
            SetWindowSize(res.w, res.h);
            if (!IsWindowFullscreen()) ToggleFullscreen();
            break;
    }

    // Apply VSync
    if (settings->vsync)
        SetWindowState(FLAG_VSYNC_HINT);
    else
        ClearWindowState(FLAG_VSYNC_HINT);

    if (screen_w) *screen_w = res.w;
    if (screen_h) *screen_h = res.h;
}
