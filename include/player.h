#pragma once

#include "raylib.h"
#include "animation.h"

// Y position of the ground surface (feet land here). Temporary — remove when tilemaps arrive.
inline constexpr float GROUND_Y     = 540.0f;
// Toggle the faint debug line drawn at GROUND_Y.
inline constexpr bool  DEBUG_GROUND = true;

enum PlayerState {
    PLAYER_IDLE,
    PLAYER_WALKING,
    PLAYER_RUNNING,
    PLAYER_JUMPING,   // ascending fast  (velocity_y < -PEAK_THRESHOLD)
    PLAYER_PEAK,      // near apex       (|velocity_y| <= PEAK_THRESHOLD)
    PLAYER_FALLING,   // descending      (velocity_y > PEAK_THRESHOLD)
    PLAYER_LANDING,   // briefly after hitting ground, plays landing frames
    PLAYER_ATTACKING
};

struct Player {
    Vector2     position;
    float       velocity_y;
    bool        grounded;
    bool        attacking;  // true while attack animation is playing
    bool        landing;    // true while landing animation is playing
    float       coyote_timer;       // allows jumping briefly after walking off a ledge
    float       jump_buffer_timer;  // queues a jump if pressed just before landing
    PlayerState state;

    Texture2D       spritesheet;     // owned — unload via player_cleanup
    Animation       anim_idle;
    Animation       anim_walk;
    Animation       anim_run;
    Animation       anim_jump_begin; // row 5, frames 0-3 — ascending
    Animation       anim_jump_peak;  // row 5, frame 4   — apex
    Animation       anim_jump_fall;  // row 5, frame 5   — descending
    Animation       anim_jump_land;  // row 5, frames 6-7 — landing
    Animation       anim_attack;
    AnimationPlayer anim_player;
};

// Load the spritesheet, define animations, place the player on the ground at start_pos.x.
void player_init(Player *player, Vector2 start_pos);

// Process input, update physics, state, and animation.
void player_update(Player *player, float dt);

// Draw the animated sprite.
void player_draw(const Player *player);

// Unload the spritesheet texture.
void player_cleanup(Player *player);
