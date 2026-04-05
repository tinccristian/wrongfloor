#include "player.h"
#include "game.h"
#include "raymath.h"
#include <cmath>
#include <algorithm>

static constexpr int   GAMEPAD_ID             = 0;
static constexpr float MOVE_SPEED             = 220.0f;
static constexpr float MOVE_THRESHOLD         = 0.15f;
static constexpr float GAMEPAD_MOVE_DEAD_ZONE = 0.2f;
static constexpr float GAMEPAD_AIM_DEAD_ZONE  = 0.25f;
static constexpr float MOUSE_SWITCH_THRESHOLD = 2.0f;
static constexpr float CROSSHAIR_DISTANCE     = 196.0f;
static constexpr float CROSSHAIR_RADIUS       = 4.0f;
static constexpr int   FRAME_W               = 16;
static constexpr int   FRAME_H               = 32;
static constexpr int   IDLE_FRAME_COUNT      = 4;
static constexpr int   RUN_FRAME_COUNT       = 6;
static constexpr int   DIRECTION_ROW_COUNT   = 5;

struct DirectionRowSelection {
    int  row    = 0;
    bool flip_h = false;
};

struct PlayerInputFrame {
    Vector2 move_input           = { 0.0f, 0.0f };
    Vector2 controller_aim_input = { 0.0f, 0.0f };
    Vector2 mouse_screen         = { 0.0f, 0.0f };
    // attack_pressed removed — weapon system reads fire input directly
};

static inline int to_tile(float px) { return (int)floorf(px / (float)TILE_SIZE); }

static Rectangle frame_rect(int col, int row)
{
    return Rectangle{
        (float)(col * FRAME_W),
        (float)(row * FRAME_H),
        (float)FRAME_W,
        (float)FRAME_H
    };
}

static void init_animation(Animation *anim, Texture2D *texture, int row, int frame_count,
                            float frame_duration, bool loops = true)
{
    anim->texture        = texture;
    anim->frame_count    = frame_count;
    anim->frame_duration = frame_duration;
    anim->loops          = loops;
    for (int i = 0; i < frame_count; ++i)
        anim->frames[i] = frame_rect(i, row);
}

Rectangle player_hitbox_rect(const Player *player)
{
    return Rectangle{
        player->position.x + HITBOX_OFFSET_X,
        player->position.y + HITBOX_OFFSET_Y,
        HITBOX_W,
        HITBOX_H
    };
}

static bool hitbox_hits_solid(const Tilemap *tm, const Rectangle& rect)
{
    if (!tm) return false;
    int start_x = to_tile(rect.x);
    int end_x   = to_tile(rect.x + rect.width  - 1.0f);
    int start_y = to_tile(rect.y);
    int end_y   = to_tile(rect.y + rect.height - 1.0f);
    for (int ty = start_y; ty <= end_y; ++ty)
    for (int tx = start_x; tx <= end_x; ++tx)
        if (tilemap_is_solid(tm, tx, ty))
            return true;
    return false;
}

static bool has_meaningful_mouse_movement(Vector2 cur, Vector2 prev)
{
    return Vector2DistanceSqr(cur, prev) >= (MOUSE_SWITCH_THRESHOLD * MOUSE_SWITCH_THRESHOLD);
}

static bool has_meaningful_controller_aim(Vector2 stick)
{
    return Vector2LengthSqr(stick) >= (GAMEPAD_AIM_DEAD_ZONE * GAMEPAD_AIM_DEAD_ZONE);
}

static FacingDirection aim_to_facing_direction(Vector2 aim_direction)
{
    float angle = atan2f(aim_direction.y, aim_direction.x) * RAD2DEG;
    if (angle < 0.0f) angle += 360.0f;
    if (angle >= 337.5f || angle < 22.5f)   return FACE_RIGHT;
    if (angle < 67.5f)                      return FACE_FRONT_RIGHT;
    if (angle < 112.5f)                     return FACE_FRONT;
    if (angle < 157.5f)                     return FACE_FRONT_LEFT;
    if (angle < 202.5f)                     return FACE_LEFT;
    if (angle < 247.5f)                     return FACE_BACK_LEFT;
    if (angle < 292.5f)                     return FACE_BACK;
    return FACE_BACK_RIGHT;
}

static DirectionRowSelection facing_to_row_selection(FacingDirection direction)
{
    switch (direction)
    {
        case FACE_FRONT:       return { 0, false };
        case FACE_FRONT_RIGHT: return { 1, false };
        case FACE_RIGHT:       return { 2, false };
        case FACE_BACK_RIGHT:  return { 3, false };
        case FACE_BACK:        return { 4, false };
        case FACE_BACK_LEFT:   return { 3, true  };
        case FACE_LEFT:        return { 2, true  };
        case FACE_FRONT_LEFT:  return { 1, true  };
        default:               return { 0, false };
    }
}

static bool animation_in_set(const Animation *anim, Animation animations[], int count)
{
    for (int i = 0; i < count; ++i)
        if (anim == &animations[i]) return true;
    return false;
}

static void set_directional_animation(Player *player, const Animation *next_animation)
{
    if (player->render.anim_player.current == next_animation) return;

    const Animation *current    = player->render.anim_player.current;
    int   preserved_frame       = player->render.anim_player.frame_index;
    float preserved_elapsed     = player->render.anim_player.elapsed;
    bool  keep_progress =
        (animation_in_set(current, player->render.anim_idle_rows, DIRECTION_ROW_COUNT) &&
         animation_in_set(next_animation, player->render.anim_idle_rows, DIRECTION_ROW_COUNT)) ||
        (animation_in_set(current, player->render.anim_run_rows,  DIRECTION_ROW_COUNT) &&
         animation_in_set(next_animation, player->render.anim_run_rows,  DIRECTION_ROW_COUNT));

    animation_player_set(&player->render.anim_player, next_animation);

    if (keep_progress && next_animation->frame_count > 0)
    {
        player->render.anim_player.frame_index =
            std::min(preserved_frame, next_animation->frame_count - 1);
        player->render.anim_player.elapsed = preserved_elapsed;
    }
}

static PlayerInputFrame sample_player_input(bool input_blocked, Vector2 virtual_mouse)
{
    PlayerInputFrame input;
    input.mouse_screen = virtual_mouse;

    if (!input_blocked)
    {
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  input.move_input.x -= 1.0f;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) input.move_input.x += 1.0f;
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    input.move_input.y -= 1.0f;
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  input.move_input.y += 1.0f;
    }

    if (!input_blocked && IsGamepadAvailable(GAMEPAD_ID))
    {
        float mx = GetGamepadAxisMovement(GAMEPAD_ID, GAMEPAD_AXIS_LEFT_X);
        float my = GetGamepadAxisMovement(GAMEPAD_ID, GAMEPAD_AXIS_LEFT_Y);
        float ax = GetGamepadAxisMovement(GAMEPAD_ID, GAMEPAD_AXIS_RIGHT_X);
        float ay = GetGamepadAxisMovement(GAMEPAD_ID, GAMEPAD_AXIS_RIGHT_Y);
        if (fabsf(mx) > GAMEPAD_MOVE_DEAD_ZONE) input.move_input.x += mx;
        if (fabsf(my) > GAMEPAD_MOVE_DEAD_ZONE) input.move_input.y += my;
        if (fabsf(ax) > GAMEPAD_AIM_DEAD_ZONE)  input.controller_aim_input.x = ax;
        if (fabsf(ay) > GAMEPAD_AIM_DEAD_ZONE)  input.controller_aim_input.y = ay;
    }

    return input;
}

static Vector2 clamp_move_input(Vector2 move_input)
{
    float len = Vector2Length(move_input);
    if (len > 1.0f) move_input = Vector2Scale(move_input, 1.0f / len);
    return move_input;
}

static void update_player_aim(Player *player, Vector2 aim_target_world,
                               const PlayerInputFrame& input)
{
    bool controller_aim_active = has_meaningful_controller_aim(input.controller_aim_input);
    bool mouse_moved           = has_meaningful_mouse_movement(input.mouse_screen,
                                     player->aim.last_mouse_screen_position);

    if (controller_aim_active)
    {
        player->aim.active_input_mode = AIM_GAMEPAD;
        player->aim.direction = Vector2Normalize(input.controller_aim_input);
    }
    else if (mouse_moved)
    {
        player->aim.active_input_mode = AIM_MOUSE_KEYBOARD;
    }

    if (player->aim.active_input_mode == AIM_MOUSE_KEYBOARD)
    {
        Vector2 to_mouse = Vector2Subtract(aim_target_world, player_center(player));
        if (Vector2LengthSqr(to_mouse) > 0.0001f)
            player->aim.direction = Vector2Normalize(to_mouse);
    }

    player->aim.angle_deg        = atan2f(player->aim.direction.y, player->aim.direction.x) * RAD2DEG;
    player->aim.facing_direction = aim_to_facing_direction(player->aim.direction);
    player->aim.last_mouse_screen_position = input.mouse_screen;
}

static void update_player_animation(Player *player, bool moving)
{
    DirectionRowSelection dr = facing_to_row_selection(player->aim.facing_direction);
    const Animation *next    = moving
        ? &player->render.anim_run_rows[dr.row]
        : &player->render.anim_idle_rows[dr.row];
    set_directional_animation(player, next);
    player->render.anim_player.flip_h = dr.flip_h;
}

static void move_and_collide(Player *player, const Tilemap *tm, Vector2 delta)
{
    if (delta.x != 0.0f)
    {
        player->position.x += delta.x;
        Rectangle hitbox = player_hitbox_rect(player);
        if (hitbox_hits_solid(tm, hitbox))
        {
            if (delta.x > 0.0f)
            {
                int tile_x = to_tile(hitbox.x + hitbox.width - 1.0f);
                player->position.x = tile_x * TILE_SIZE - HITBOX_W - HITBOX_OFFSET_X;
            }
            else
            {
                int tile_x = to_tile(hitbox.x);
                player->position.x = (tile_x + 1) * TILE_SIZE - HITBOX_OFFSET_X;
            }
        }
    }

    if (delta.y != 0.0f)
    {
        player->position.y += delta.y;
        Rectangle hitbox = player_hitbox_rect(player);
        if (hitbox_hits_solid(tm, hitbox))
        {
            if (delta.y > 0.0f)
            {
                int tile_y = to_tile(hitbox.y + hitbox.height - 1.0f);
                player->position.y = tile_y * TILE_SIZE - HITBOX_H - HITBOX_OFFSET_Y;
            }
            else
            {
                int tile_y = to_tile(hitbox.y);
                player->position.y = (tile_y + 1) * TILE_SIZE - HITBOX_OFFSET_Y;
            }
        }
    }
}

void player_init(Player *player, Vector2 start_pos)
{
    player->position = {
        start_pos.x - HITBOX_OFFSET_X,
        start_pos.y - HITBOX_OFFSET_Y - HITBOX_H
    };
    player->velocity_x                    = 0.0f;
    player->velocity_y                    = 0.0f;
    player->aim.direction                 = { 1.0f, 0.0f };
    player->aim.angle_deg                 = 0.0f;
    player->aim.active_input_mode         = AIM_MOUSE_KEYBOARD;
    player->aim.last_mouse_screen_position = { 0.0f, 0.0f };
    player->aim.facing_direction           = FACE_FRONT;
    player->state      = PLAYER_IDLE;
    player->prev_state = PLAYER_IDLE;
    player->prev_anim_frame = -1;

    player->render.idle_sheet = LoadTexture(assets_path("character/idle.png").c_str());
    player->render.run_sheet  = LoadTexture(assets_path("character/run.png").c_str());

    for (int row = 0; row < DIRECTION_ROW_COUNT; ++row)
    {
        init_animation(&player->render.anim_idle_rows[row], &player->render.idle_sheet,
                       row, IDLE_FRAME_COUNT, 0.18f);
        init_animation(&player->render.anim_run_rows[row],  &player->render.run_sheet,
                       row, RUN_FRAME_COUNT,  0.10f);
    }

    animation_player_set(&player->render.anim_player, &player->render.anim_idle_rows[0]);
    player->render.anim_player.flip_h = false;
}

void player_update(Player *player, const Tilemap *tm, float dt,
                   Vector2 aim_target_world, Vector2 virtual_mouse,
                   PlayerSoundTriggers *triggers, bool input_blocked)
{
    *triggers = PlayerSoundTriggers{};
    PlayerInputFrame input  = sample_player_input(input_blocked, virtual_mouse);
    Vector2 move_input      = clamp_move_input(input.move_input);
    float   input_length    = Vector2Length(move_input);

    bool   moving      = input_length > MOVE_THRESHOLD;
    float  speed_scale = std::min(input_length, 1.0f);
    Vector2 velocity   = moving
        ? Vector2Scale(move_input, MOVE_SPEED * speed_scale)
        : Vector2{ 0.0f, 0.0f };

    player->velocity_x = velocity.x;
    player->velocity_y = velocity.y;

    move_and_collide(player, tm, { player->velocity_x * dt, player->velocity_y * dt });
    update_player_aim(player, aim_target_world, input);

    PlayerState state_before = player->state;
    player->state = moving ? PLAYER_RUNNING : PLAYER_IDLE;

    update_player_animation(player, moving);

    int frame_before = player->render.anim_player.frame_index;
    animation_player_update(&player->render.anim_player, dt);
    int frame_after  = player->render.anim_player.frame_index;

    if (frame_after != frame_before &&
        animation_in_set(player->render.anim_player.current,
                         player->render.anim_run_rows, DIRECTION_ROW_COUNT))
    {
        if (frame_after == 0 || frame_after == 3)
            triggers->footstep_run = true;
    }

    if (state_before != player->state && player->state == PLAYER_RUNNING)
        triggers->footstep_run = true;

    player->prev_state      = state_before;
    player->prev_anim_frame = frame_after;
}

void player_prepare_draw(Player *player)
{
    (void)player; // attack shader removed; nothing to prepare
}

void player_draw(Player *player)
{
    animation_player_draw(&player->render.anim_player, player->position, SPRITE_SCALE);
}

Vector2 player_crosshair_position(const Player *player)
{
    return Vector2Add(player_center(player),
                      Vector2Scale(player->aim.direction, CROSSHAIR_DISTANCE));
}

void player_draw_crosshair(const Player *player)
{
    DrawCircleV(player_crosshair_position(player), CROSSHAIR_RADIUS, WHITE);
}

void player_cleanup(Player *player)
{
    if (player->render.idle_sheet.id != 0) UnloadTexture(player->render.idle_sheet);
    if (player->render.run_sheet.id  != 0) UnloadTexture(player->render.run_sheet);
    player->render.idle_sheet = Texture2D{};
    player->render.run_sheet  = Texture2D{};
}

Vector2 player_center(const Player *player)
{
    return { player->position.x + SPRITE_W * 0.5f,
             player->position.y + SPRITE_H * 0.5f };
}
