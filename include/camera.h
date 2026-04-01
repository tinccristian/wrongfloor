#pragma once

#include "raylib.h"
#include "tilemap.h"

// Smoothly-following Camera2D that clamps to map bounds.
struct GameCamera {
    Camera2D cam{};
    float    smooth = 8.0f; // lerp speed (higher = snappier)
};

// Initialise the camera centred on target with screen dimensions.
void camera_init(GameCamera *gc, Vector2 target, int screen_w, int screen_h);

// Lerp toward target and clamp so the view doesn't exceed map bounds.
void camera_update(GameCamera *gc, Vector2 target, const Tilemap *tm,
                   int screen_w, int screen_h, float dt);
