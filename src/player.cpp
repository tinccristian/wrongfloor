#include "player.h"
#include <cmath>

static constexpr int   GAMEPAD_ID            = 0;
static constexpr float WALK_SPEED            = 150.0f;
static constexpr float RUN_SPEED             = 300.0f;
static constexpr float SPRITE_SCALE          = 3.0f;
static constexpr float GAMEPAD_DEAD_ZONE     = 0.1f;
static constexpr float GAMEPAD_RUN_THRESHOLD = 0.7f;

static Rectangle frame_rect(int col, int row)
{
    return Rectangle{ (float)(col * 32), (float)(row * 32), 32.0f, 32.0f };
}

void player_init(Player *player, Vector2 start_pos)
{
    player->position = start_pos;
    player->state    = PLAYER_IDLE;

    player->spritesheet = LoadTexture("assets/character.png");

    // Idle: 4 frames spanning rows 0 and 1 (cols 0-1 of each)
    player->anim_idle.texture       = &player->spritesheet;
    player->anim_idle.frame_count   = 4;
    player->anim_idle.frame_duration = 0.15f;
    player->anim_idle.loops         = true;
    player->anim_idle.frames[0]     = frame_rect(0, 0);
    player->anim_idle.frames[1]     = frame_rect(1, 0);
    player->anim_idle.frames[2]     = frame_rect(0, 1);
    player->anim_idle.frames[3]     = frame_rect(1, 1);

    // Walk: 4 frames from row 2
    player->anim_walk.texture        = &player->spritesheet;
    player->anim_walk.frame_count    = 4;
    player->anim_walk.frame_duration = 0.1f;
    player->anim_walk.loops          = true;
    for (int i = 0; i < 4; i++)
        player->anim_walk.frames[i] = frame_rect(i, 2);

    // Run: 8 frames from row 3
    player->anim_run.texture        = &player->spritesheet;
    player->anim_run.frame_count    = 8;
    player->anim_run.frame_duration = 0.08f;
    player->anim_run.loops          = true;
    for (int i = 0; i < 8; i++)
        player->anim_run.frames[i] = frame_rect(i, 3);

    animation_player_set(&player->anim_player, &player->anim_idle);
}

void player_update(Player *player, float dt)
{
    // ── Keyboard ──────────────────────────────────────────
    bool kb_left  = IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A);
    bool kb_right = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
    bool kb_run   = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

    float dir_x = 0.0f;
    bool  run   = false;

    if (kb_left)  dir_x -= 1.0f;
    if (kb_right) dir_x += 1.0f;
    if (dir_x != 0.0f && kb_run) run = true;

    // ── Gamepad ───────────────────────────────────────────
    if (IsGamepadAvailable(GAMEPAD_ID))
    {
        float ax = GetGamepadAxisMovement(GAMEPAD_ID, GAMEPAD_AXIS_LEFT_X);
        if (fabsf(ax) > GAMEPAD_DEAD_ZONE)
        {
            dir_x += ax;
            if (fabsf(ax) > GAMEPAD_RUN_THRESHOLD) run = true;
        }
        if (IsGamepadButtonDown(GAMEPAD_ID, GAMEPAD_BUTTON_LEFT_FACE_LEFT))  dir_x -= 1.0f;
        if (IsGamepadButtonDown(GAMEPAD_ID, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) dir_x += 1.0f;
    }

    // Clamp to [-1, 1]
    if (dir_x >  1.0f) dir_x =  1.0f;
    if (dir_x < -1.0f) dir_x = -1.0f;

    // Derive state
    PlayerState new_state = PLAYER_IDLE;
    if (dir_x != 0.0f)
        new_state = run ? PLAYER_RUNNING : PLAYER_WALKING;

    player->state = new_state;

    // Flip sprite based on last movement direction
    if (dir_x < 0.0f) player->anim_player.flip_h = true;
    if (dir_x > 0.0f) player->anim_player.flip_h = false;

    // Switch animation when state changes (no-op if already the same)
    switch (player->state)
    {
        case PLAYER_IDLE:    animation_player_set(&player->anim_player, &player->anim_idle); break;
        case PLAYER_WALKING: animation_player_set(&player->anim_player, &player->anim_walk); break;
        case PLAYER_RUNNING: animation_player_set(&player->anim_player, &player->anim_run);  break;
    }

    // Move on X axis only (Y is locked to start position for now)
    float speed = (player->state == PLAYER_RUNNING) ? RUN_SPEED : WALK_SPEED;
    player->position.x += dir_x * speed * dt;

    animation_player_update(&player->anim_player, dt);
}

void player_draw(const Player *player)
{
    animation_player_draw(&player->anim_player, player->position, SPRITE_SCALE);
}

void player_cleanup(Player *player)
{
    UnloadTexture(player->spritesheet);
}
