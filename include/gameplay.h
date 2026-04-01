#pragma once

#include "game.h"

// Initialise gameplay state and load the starting level.
void gameplay_init(GameState *state, int screen_w, int screen_h);

// Advance one gameplay frame, including player, bullets, camera, audio, and transitions.
void gameplay_update(GameState *state, float dt, int screen_w, int screen_h, bool input_blocked);

// Prepare offscreen render resources needed by the current frame before visible drawing begins.
void gameplay_prepare_draw(GameState *state);

// Draw world-space gameplay layers and entities inside an active Camera2D pass.
void gameplay_draw_world(GameState *state);

// Release gameplay-owned resources.
void gameplay_cleanup(GameState *state);
