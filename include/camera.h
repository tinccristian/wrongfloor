#pragma once

#include "raylib.h"
#include "tilemap.h"

// Camera2D centered on the player and clamped to map bounds.
struct GameCamera {
    Camera2D cam{};
};

// Initialise the camera centred on target with screen dimensions.
void camera_init(GameCamera *gc, Vector2 target, int screen_w, int screen_h);

// Center on target and clamp so the view doesn't exceed map bounds.
void camera_update(GameCamera *gc, Vector2 target, const Tilemap *tm,
                   int screen_w, int screen_h, float dt);
