#pragma once

#include "player.h"
#include "bullets.h"
#include "tilemap.h"
#include "camera.h"
#include "audio.h"
#include "enemy.h"
#include "effects.h"
#include "weapon_manager.h"
#include "replay.h"
#include "settings.h"
#include <string>

// Game state machine: which screen/mode are we in?
enum class GameStateMode {
    MAIN_MENU,        // Start here
    PLAYING,          // Active gameplay
    PAUSED,           // Paused during gameplay (pause menu open)
    OPTIONS_MAIN,     // Options menu from main menu
    OPTIONS_PAUSE     // Options menu from pause menu
};

// Returns the base asset directory: the source folder in dev builds (instant
// iteration), or the build-adjacent "assets/" folder in release builds.
inline std::string assets_path(const std::string& rel)
{
#ifdef ASSETS_PATH
    return std::string(ASSETS_PATH) + rel;
#else
    return std::string("assets/") + rel;
#endif
}

// Virtual (design) resolution — all game logic and rendering operates in this space.
// The final frame is scaled to fit the actual window with letterboxing.
inline constexpr int VIRTUAL_W = 1280;
inline constexpr int VIRTUAL_H = 720;

struct GameState {
    Player        player;
    BulletSystem  bullets;
    Tilemap       tilemap;
    GameCamera    camera;
    AudioState    audio;
    EnemyManager  enemies;
    EffectsSystem effects;
    WeaponManager weapons;
    ReplaySystem  replay;
    Settings      settings;

    // Game state machine
    GameStateMode mode = GameStateMode::MAIN_MENU;

    // Level cycling: current level index (0 = level_01, 1 = level_02)
    int current_level = 0;

    bool paused      = false;
    bool player_dead = false;  // true while slow-mo or replay is running

    // Slow-motion: gameplay dt is multiplied by time_scale after a death hit.
    // main.cpp drives death_slowmo_timer in real time; resets to 1.0 after transition.
    float time_scale          = 1.0f;
    float death_slowmo_timer  = 0.0f;

    // Mouse position transformed into virtual 1280x720 space.
    // Computed once per frame in main.cpp before any update calls.
    Vector2 virtual_mouse = { 0.0f, 0.0f };
};

// Level file names (relative to assets root — prepend via assets_path())
inline const char* LEVELS[] = { "levels/level_01.tmj", "levels/level_02.tmj" };
inline constexpr int LEVEL_COUNT = 2;
