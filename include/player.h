#pragma once

#include "raylib.h"
#include "animation.h"

enum PlayerState {
    PLAYER_IDLE,
    PLAYER_WALKING,
    PLAYER_RUNNING
};

struct Player {
    Vector2     position;
    PlayerState state;

    Texture2D       spritesheet;   // owned — unload via player_cleanup
    Animation       anim_idle;
    Animation       anim_walk;
    Animation       anim_run;
    AnimationPlayer anim_player;
};

// Load the spritesheet, define animations, place the player at start_pos.
void player_init(Player *player, Vector2 start_pos);

// Process input, update state and animation.
void player_update(Player *player, float dt);

// Draw the animated sprite.
void player_draw(const Player *player);

// Unload the spritesheet texture.
void player_cleanup(Player *player);
