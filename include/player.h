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
    PLAYER_WALKING,
    PLAYER_RUNNING,
    PLAYER_ATTACKING
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
    bool footstep_walk = false; // walk footstep frame crossed
    bool footstep_run  = false; // run footstep frame crossed
    bool landed        = false; // reserved for future use
    bool attacked      = false; // attack fired this frame
    Vector2 attack_origin = {};
    Vector2 attack_direction = { 1.0f, 0.0f };
};

struct Player {
    Vector2     position{};      // top-left of the sprite in world space
    float       velocity_x = 0.0f;
    float       velocity_y = 0.0f;
    Vector2     aim_direction = { 1.0f, 0.0f };
    float       aim_angle_deg = 0.0f;
    AimInputMode active_aim_mode = AIM_MOUSE_KEYBOARD;
    Vector2     last_mouse_screen_position{};
    FacingDirection facing_direction = FACE_FRONT;
    bool        attacking     = false;
    float       attack_timer  = 0.0f;
    PlayerState state         = PLAYER_IDLE;
    PlayerState prev_state    = PLAYER_IDLE;
    int         prev_anim_frame = -1; // previous frame index, for footstep detection

    Texture2D       idle_sheet{};
    Texture2D       run_sheet{};
    Animation       anim_idle_rows[5]{};
    Animation       anim_run_rows[5]{};
    AnimationPlayer anim_player{};

    RenderTexture2D attack_render_target{};
    Shader          attack_shader{};
    int             attack_shader_strength_loc = -1;
    int             attack_shader_anchor_loc = -1;
};

// Load top-down directional player animations and place the player at start_pos.
void player_init(Player *player, Vector2 start_pos);

// Process input, move with top-down wall collision, and update mixed-device aiming.
// Fills *triggers with sound events that fired this frame.
// Pass input_blocked=true to suppress movement input while preserving aiming.
void player_update(Player *player, const Tilemap *tm, float dt,
                   Vector2 aim_target_world, PlayerSoundTriggers *triggers,
                   bool input_blocked = false);

// Prepare any offscreen player rendering needed for the current frame.
void player_prepare_draw(Player *player);

// Draw the animated sprite at the player position.
void player_draw(Player *player);

// Draw a fixed-distance world-space aim marker from the current aim direction.
void player_draw_crosshair(const Player *player);

// Unload player textures.
void player_cleanup(Player *player);

// Returns the world-space visual centre of the character sprite.
Vector2 player_center(const Player *player);

// Returns the world-space point bullets should spawn from.
Vector2 player_attack_origin(const Player *player);

// Returns the world-space position of the current aim marker.
Vector2 player_crosshair_position(const Player *player);
