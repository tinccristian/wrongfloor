#pragma once

#include "raylib.h"
#include "animation.h"
#include "tilemap.h"

// Sprite render scale
inline constexpr float SPRITE_SCALE  = 3.0f;
// Sprite frame size in world pixels
inline constexpr float SPRITE_PX     = 32.0f * SPRITE_SCALE; // 96

// Hitbox within the sprite (world pixels) — smaller than the sprite for fair collisions.
inline constexpr float HITBOX_W        = 48.0f; // 16 sprite-px * 3
inline constexpr float HITBOX_H        = 75.0f; // 25 sprite-px * 3
inline constexpr float HITBOX_OFFSET_X = 24.0f; // centred in 96px-wide sprite
inline constexpr float HITBOX_OFFSET_Y = 18.0f; // slight top inset

enum PlayerState {
    PLAYER_IDLE,
    PLAYER_WALKING,
    PLAYER_RUNNING,
    PLAYER_JUMPING,   // ascending fast  (velocity_y < -PEAK_THRESHOLD)
    PLAYER_PEAK,      // near apex       (|velocity_y| <= PEAK_THRESHOLD)
    PLAYER_FALLING,   // descending      (velocity_y > PEAK_THRESHOLD)
    PLAYER_LANDING,   // briefly after hitting ground
    PLAYER_ATTACKING
};

// Sound events produced by player_update for the caller to act on.
struct PlayerSoundTriggers {
    bool footstep_walk = false; // walk footstep frame crossed
    bool footstep_run  = false; // run footstep frame crossed
    bool landed        = false; // just touched ground from air
    bool attacked      = false; // attack just started
};

struct Player {
    Vector2     position;   // top-left of the 96×96 sprite
    float       velocity_y;
    bool        grounded;
    bool        attacking;
    bool        landing;
    float       coyote_timer;
    float       jump_buffer_timer;
    bool        double_jumped;  // true after the air jump has been used
    PlayerState state;
    int         prev_anim_frame = -1; // previous frame index, for footstep detection

    Texture2D       spritesheet;
    Animation       anim_idle;
    Animation       anim_walk;
    Animation       anim_run;
    Animation       anim_jump_begin;
    Animation       anim_jump_peak;
    Animation       anim_jump_fall;
    Animation       anim_jump_land;
    Animation       anim_attack;
    AnimationPlayer anim_player;
};

// Load spritesheet, define animations, place player at start_pos (world coords).
void player_init(Player *player, Vector2 start_pos);

// Process input, run tile collision, update state and animation.
// Fills *triggers with sound events that fired this frame.
void player_update(Player *player, const Tilemap *tm, float dt, PlayerSoundTriggers *triggers);

// Draw the animated sprite.
void player_draw(const Player *player);

// Unload the spritesheet texture.
void player_cleanup(Player *player);

// Returns the world-space centre of the player hitbox (useful for the camera).
Vector2 player_center(const Player *player);
