#pragma once

#include "player.h"
#include "bullets.h"
#include "tilemap.h"
#include "camera.h"
#include "audio.h"
#include "enemy.h"
#include "effects.h"
#include <string>

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

struct GameState {
    Player        player;
    BulletSystem  bullets;
    Tilemap       tilemap;
    GameCamera    camera;
    AudioState    audio;
    EnemyManager  enemies;
    EffectsSystem effects;

    // Level cycling: current level index (0 = level_01, 1 = level_02)
    int current_level = 0;

    // FUTURE: to add pause/menu/game-over states, introduce an AppState enum here
    // (e.g. APP_PLAYING, APP_PAUSED, APP_MENU) and gate gameplay_update/draw behind it.
    // gameplay_update already receives input_blocked; a similar flag could gate drawing.
};

// Level file names (relative to assets root — prepend via assets_path())
inline const char* LEVELS[] = { "level_01.tmj", "level_02.tmj" };
inline constexpr int LEVEL_COUNT = 2;
