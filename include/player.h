#pragma once

#include "raylib.h"

typedef struct {
    Vector2 position;
    float speed;
    float size;
    Color color;
} Player;

// Initialize player at a given world position
void player_init(Player *player, Vector2 start_pos);

// Handle input and update player state for this frame
void player_update(Player *player, float dt);

// Draw the player
void player_draw(const Player *player);
