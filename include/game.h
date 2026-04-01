#pragma once

#include "player.h"
#include "tilemap.h"
#include "camera.h"
#include "audio.h"
#include <string>

struct GameState {
    Player     player;
    Tilemap    tilemap;
    GameCamera camera;
    AudioState audio;

    // Level cycling: current level index (0 = level_01, 1 = level_02)
    int current_level = 0;
};

// Level file paths
inline const char* LEVELS[] = { "assets/level_01.tmj", "assets/level_02.tmj" };
inline constexpr int LEVEL_COUNT = 2;
