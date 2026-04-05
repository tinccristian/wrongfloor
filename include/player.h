#pragma once

#include "raylib.h"
#include "animation.h"
#include "tilemap.h"

// Character sprite render scale.
inline constexpr float SPRITE_SCALE = 3.0f;
// Sprite frame size in world pixels.
inline constexpr float SPRITE_W = 16.0f * SPRITE_SCALE; // 48
inline constexpr float SPRITE_H = 32.0f * SPRITE_SCALE; // 96

// Small footprint used for top-down wall collision.
inline constexpr float HITBOX_W        = 24.0f;
inline constexpr float HITBOX_H        = 24.0f;
inline constexpr float HITBOX_OFFSET_X = (SPRITE_W - HITBOX_W) * 0.5f;
inline constexpr float HITBOX_OFFSET_Y = SPRITE_H - HITBOX_H - 8.0f;

enum PlayerState {
    PLAYER_IDLE,
    PLAYER_WALKING, // NOTE: currently unused — kept for future state machine use
    PLAYER_RUNNING
    // PLAYER_ATTACKING removed — shooting is now handled by the weapon system
};

enum FacingDirection {
    FACE_FRONT,
    FACE_FRONT_RIGHT,
    FACE_RIGHT,
    FACE_BACK_RIGHT,
    FACE_BACK,
    FACE_BACK_LEFT,
    FACE_LEFT,
    FACE_FRONT_LEFT
};

enum AimInputMode {
    AIM_MOUSE_KEYBOARD,
    AIM_GAMEPAD
};

// Sound events produced by player_update for the caller to act on.
struct PlayerSoundTriggers {
    bool footstep_walk = false;
    bool footstep_run  = false;
    bool landed        = false; // reserved
};

// Runtime state for aim ownership, resolved direction, and facing.
struct PlayerAimState {
    Vector2      direction = { 1.0f, 0.0f };
    float        angle_deg = 0.0f;
    AimInputMode active_input_mode = AIM_MOUSE_KEYBOARD;
    Vector2      last_mouse_screen_position{};
    FacingDirection facing_direction = FACE_FRONT;
};

// Player-owned render resources and animation playback state.
struct PlayerRenderState {
    Texture2D       idle_sheet{};
    Texture2D       run_sheet{};
    Animation       anim_idle_rows[5]{};
    Animation       anim_run_rows[5]{};
    AnimationPlayer anim_player{};
};

struct Player {
    Vector2     position{};      // top-left of the sprite in world space
    float       velocity_x = 0.0f;
    float       velocity_y = 0.0f;
    PlayerAimState aim{};
    PlayerState state         = PLAYER_IDLE;
    PlayerState prev_state    = PLAYER_IDLE;
    int         prev_anim_frame = -1;

    PlayerRenderState render{};
};

// Load top-down directional player animations and place the player at start_pos.
void player_init(Player *player, Vector2 start_pos);

// Process input, move with top-down wall collision, and update mixed-device aiming.
// Fills *triggers with sound events that fired this frame.
// Pass input_blocked=true to suppress movement input while preserving aiming.
void player_update(Player *player, const Tilemap *tm, float dt,
                   Vector2 aim_target_world, Vector2 virtual_mouse,
                   PlayerSoundTriggers *triggers, bool input_blocked = false);

// No-op — retained for API consistency. Call before BeginDrawing each frame.
void player_prepare_draw(Player *player);

// Draw the animated sprite at the player position.
void player_draw(Player *player);

// Draw a fixed-distance world-space aim marker from the current aim direction.
void player_draw_crosshair(const Player *player);

// Unload player textures.
void player_cleanup(Player *player);

// Returns the world-space visual centre of the character sprite.
Vector2 player_center(const Player *player);

// Returns the axis-aligned bounding box used for wall collision and overlap tests.
Rectangle player_hitbox_rect(const Player *player);

// Returns the world-space position of the current aim marker.
Vector2 player_crosshair_position(const Player *player);
