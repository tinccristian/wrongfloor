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

// Draw screen-space HUD elements (ammo counter, etc.). Call outside BeginMode2D.
void gameplay_draw_hud(GameState *state, int screen_w, int screen_h);

// Release gameplay-owned resources.
void gameplay_cleanup(GameState *state);

// Reload the current level from scratch (used by pause menu Restart).
void gameplay_reload_level(GameState *state, int screen_w, int screen_h);

// Load a specific level by index (used by pause menu Main Menu → level 0).
void gameplay_load_level(GameState *state, int index, int screen_w, int screen_h);
