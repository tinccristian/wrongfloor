#include "player.h"
#include <cmath>

static constexpr int   GAMEPAD_ID            = 0;
static constexpr float WALK_SPEED            = 150.0f;
static constexpr float RUN_SPEED             = 300.0f;
static constexpr float SPRITE_SCALE          = 3.0f;
static constexpr float SPRITE_HEIGHT         = 32.0f * SPRITE_SCALE; // 96 px
static constexpr float GAMEPAD_DEAD_ZONE     = 0.1f;
static constexpr float GAMEPAD_RUN_THRESHOLD = 0.7f;

// ── Jump physics (Celeste-inspired) ──────────────────────────────────────────
// Asymmetric gravity: fall twice as fast as you rise for a snappier arc.
static constexpr float GRAVITY_UP     = 1500.0f; // px/s² while ascending
static constexpr float GRAVITY_DOWN   = 3000.0f; // px/s² while descending
static constexpr float JUMP_VELOCITY  = -650.0f; // initial upward velocity
static constexpr float JUMP_CUT_VY   = 300.0f;  // if jump released early, cap to -JUMP_CUT_VY

// Coyote time: can still jump briefly after walking off a ledge.
static constexpr float COYOTE_TIME       = 0.10f; // seconds
// Jump buffer: queues a jump if pressed just before landing.
static constexpr float JUMP_BUFFER_TIME  = 0.12f; // seconds
// Velocity window around zero that shows the peak animation frame.
static constexpr float PEAK_THRESHOLD    = 100.0f; // px/s

static Rectangle frame_rect(int col, int row)
{
    return Rectangle{ (float)(col * 32), (float)(row * 32), 32.0f, 32.0f };
}

// Helper: build a non-looping animation from a contiguous range of frames on one row.
static void anim_init(Animation *a, Texture2D *tex, int row, int start_col, int count, float dur)
{
    a->texture        = tex;
    a->frame_count    = count;
    a->frame_duration = dur;
    a->loops          = false;
    for (int i = 0; i < count; i++)
        a->frames[i] = frame_rect(start_col + i, row);
}

void player_init(Player *player, Vector2 start_pos)
{
    player->position          = { start_pos.x, GROUND_Y - SPRITE_HEIGHT };
    player->velocity_y        = 0.0f;
    player->grounded          = true;
    player->attacking         = false;
    player->landing           = false;
    player->coyote_timer      = 0.0f;
    player->jump_buffer_timer = 0.0f;
    player->state             = PLAYER_IDLE;

    player->spritesheet = LoadTexture("assets/character.png");
    Texture2D *tex = &player->spritesheet;

    // Idle: 4 frames spanning rows 0-1 (looping)
    player->anim_idle.texture        = tex;
    player->anim_idle.frame_count    = 4;
    player->anim_idle.frame_duration = 0.15f;
    player->anim_idle.loops          = true;
    player->anim_idle.frames[0]      = frame_rect(0, 0);
    player->anim_idle.frames[1]      = frame_rect(1, 0);
    player->anim_idle.frames[2]      = frame_rect(0, 1);
    player->anim_idle.frames[3]      = frame_rect(1, 1);

    // Walk: row 2, 4 frames (looping)
    player->anim_walk.texture        = tex;
    player->anim_walk.frame_count    = 4;
    player->anim_walk.frame_duration = 0.1f;
    player->anim_walk.loops          = true;
    for (int i = 0; i < 4; i++)
        player->anim_walk.frames[i] = frame_rect(i, 2);

    // Run: row 3, 8 frames (looping)
    player->anim_run.texture        = tex;
    player->anim_run.frame_count    = 8;
    player->anim_run.frame_duration = 0.08f;
    player->anim_run.loops          = true;
    for (int i = 0; i < 8; i++)
        player->anim_run.frames[i] = frame_rect(i, 3);

    // Jump sub-animations — all non-looping, row 5:
    anim_init(&player->anim_jump_begin, tex, 5, 0, 4, 0.07f); // frames 0-3: ascending
    anim_init(&player->anim_jump_peak,  tex, 5, 4, 1, 0.20f); // frame 4: apex
    anim_init(&player->anim_jump_fall,  tex, 5, 5, 1, 0.15f); // frame 5: falling
    anim_init(&player->anim_jump_land,  tex, 5, 6, 2, 0.05f); // frames 6-7: landing

    // Attack: row 8, 8 frames, fast
    anim_init(&player->anim_attack, tex, 8, 0, 8, 0.05f);

    animation_player_set(&player->anim_player, &player->anim_idle);
}

void player_update(Player *player, float dt)
{
    // ── Capture last-frame state before anything changes ──────────────
    bool was_in_air = (player->state == PLAYER_JUMPING ||
                       player->state == PLAYER_PEAK    ||
                       player->state == PLAYER_FALLING);

    // ── Input ──────────────────────────────────────────────────────────
    bool kb_left        = IsKeyDown(KEY_LEFT)      || IsKeyDown(KEY_A);
    bool kb_right       = IsKeyDown(KEY_RIGHT)     || IsKeyDown(KEY_D);
    bool kb_run         = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    bool jump_pressed   = IsKeyPressed(KEY_SPACE);
    bool jump_held      = IsKeyDown(KEY_SPACE);
    bool attack_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsKeyPressed(KEY_X);

    float dir_x = 0.0f;
    bool  run   = false;

    if (kb_left)  dir_x -= 1.0f;
    if (kb_right) dir_x += 1.0f;
    if (dir_x != 0.0f && kb_run) run = true;

    if (IsGamepadAvailable(GAMEPAD_ID))
    {
        float ax = GetGamepadAxisMovement(GAMEPAD_ID, GAMEPAD_AXIS_LEFT_X);
        if (fabsf(ax) > GAMEPAD_DEAD_ZONE)
        {
            dir_x += ax;
            if (fabsf(ax) > GAMEPAD_RUN_THRESHOLD) run = true;
        }
        if (IsGamepadButtonDown(GAMEPAD_ID,    GAMEPAD_BUTTON_LEFT_FACE_LEFT))   dir_x -= 1.0f;
        if (IsGamepadButtonDown(GAMEPAD_ID,    GAMEPAD_BUTTON_LEFT_FACE_RIGHT))  dir_x += 1.0f;
        if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))  jump_pressed   = true;
        if (IsGamepadButtonDown(GAMEPAD_ID,    GAMEPAD_BUTTON_RIGHT_FACE_DOWN))  jump_held      = true;
        if (IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_RIGHT_FACE_LEFT))  attack_pressed = true;
    }

    if (dir_x >  1.0f) dir_x =  1.0f;
    if (dir_x < -1.0f) dir_x = -1.0f;

    // ── Variable jump height (cut upward velocity on early release) ───
    if (!player->grounded && !jump_held && player->velocity_y < -JUMP_CUT_VY)
        player->velocity_y = -JUMP_CUT_VY;

    // ── Asymmetric gravity ────────────────────────────────────────────
    float grav = (player->velocity_y < 0.0f) ? GRAVITY_UP : GRAVITY_DOWN;
    player->grounded    = false;
    player->velocity_y += grav * dt;
    player->position.y += player->velocity_y * dt;

    // ── Ground snap — temporary; replace with tilemap collision later ─
    if (player->position.y + SPRITE_HEIGHT >= GROUND_Y)
    {
        player->position.y = GROUND_Y - SPRITE_HEIGHT;
        player->velocity_y = 0.0f;
        player->grounded   = true;
    }

    // ── Coyote time ───────────────────────────────────────────────────
    if (player->grounded)
        player->coyote_timer = COYOTE_TIME;
    else
        player->coyote_timer = fmaxf(0.0f, player->coyote_timer - dt);

    bool can_jump = player->coyote_timer > 0.0f;

    // ── Jump buffer ───────────────────────────────────────────────────
    if (jump_pressed)
        player->jump_buffer_timer = JUMP_BUFFER_TIME;
    else
        player->jump_buffer_timer = fmaxf(0.0f, player->jump_buffer_timer - dt);

    // ── Execute jump (buffered or fresh) ──────────────────────────────
    if (player->jump_buffer_timer > 0.0f && can_jump)
    {
        player->velocity_y        = JUMP_VELOCITY;
        player->grounded          = false;
        player->coyote_timer      = 0.0f; // prevent double-jump via coyote
        player->jump_buffer_timer = 0.0f;
        player->landing           = false; // cut short any landing anim
    }

    // ── Advance animation (before state machine so we can detect anim end) ──
    animation_player_update(&player->anim_player, dt);

    // ── Landing trigger ───────────────────────────────────────────────
    if (player->grounded && was_in_air && !player->attacking)
        player->landing = true;

    // Cancel landing if we fell off a ledge mid-animation
    if (player->landing && !player->grounded)
        player->landing = false;

    // Exit landing when its 2-frame animation finishes
    if (player->landing)
    {
        bool done = (player->anim_player.current    == &player->anim_jump_land) &&
                    (player->anim_player.frame_index >= player->anim_jump_land.frame_count - 1);
        if (done) player->landing = false;
    }

    // ── Attack management ─────────────────────────────────────────────
    if (player->attacking)
    {
        bool done = (player->anim_player.current    == &player->anim_attack) &&
                    (player->anim_player.frame_index >= player->anim_attack.frame_count - 1);
        if (done) player->attacking = false;
    }

    if (!player->attacking && attack_pressed)
    {
        player->attacking = true;
        player->landing   = false; // attack cancels landing anim
        animation_player_set(&player->anim_player, &player->anim_attack);
    }

    // ── Determine movement state (physics-based) ──────────────────────
    PlayerState move_state;
    if (!player->grounded)
    {
        if      (player->velocity_y < -PEAK_THRESHOLD) move_state = PLAYER_JUMPING;
        else if (player->velocity_y >  PEAK_THRESHOLD) move_state = PLAYER_FALLING;
        else                                            move_state = PLAYER_PEAK;
    }
    else if (dir_x != 0.0f)
        move_state = run ? PLAYER_RUNNING : PLAYER_WALKING;
    else
        move_state = PLAYER_IDLE;

    // ── Apply state and animation ─────────────────────────────────────
    if (player->attacking)
    {
        player->state = PLAYER_ATTACKING;
        // anim_attack already set; don't switch
    }
    else if (player->landing)
    {
        player->state = PLAYER_LANDING;
        animation_player_set(&player->anim_player, &player->anim_jump_land);
    }
    else
    {
        player->state = move_state;
        switch (move_state)
        {
            case PLAYER_IDLE:    animation_player_set(&player->anim_player, &player->anim_idle);       break;
            case PLAYER_WALKING: animation_player_set(&player->anim_player, &player->anim_walk);       break;
            case PLAYER_RUNNING: animation_player_set(&player->anim_player, &player->anim_run);        break;
            case PLAYER_JUMPING: animation_player_set(&player->anim_player, &player->anim_jump_begin); break;
            case PLAYER_PEAK:    animation_player_set(&player->anim_player, &player->anim_jump_peak);  break;
            case PLAYER_FALLING: animation_player_set(&player->anim_player, &player->anim_jump_fall);  break;
            default: break;
        }
    }

    // ── Flip and horizontal movement ──────────────────────────────────
    if (dir_x < 0.0f) player->anim_player.flip_h = true;
    if (dir_x > 0.0f) player->anim_player.flip_h = false;

    float speed = (move_state == PLAYER_RUNNING) ? RUN_SPEED : WALK_SPEED;
    player->position.x += dir_x * speed * dt;
}

void player_draw(const Player *player)
{
    animation_player_draw(&player->anim_player, player->position, SPRITE_SCALE);
}

void player_cleanup(Player *player)
{
    UnloadTexture(player->spritesheet);
}
