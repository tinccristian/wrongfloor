#include "player.h"
#include "game.h"
#include <cmath>

static constexpr int   GAMEPAD_ID            = 0;
static constexpr float WALK_SPEED            = 150.0f;
static constexpr float RUN_SPEED             = 300.0f;
static constexpr float GAMEPAD_DEAD_ZONE     = 0.1f;
static constexpr float GAMEPAD_RUN_THRESHOLD = 0.7f;

// ── Jump physics (Celeste-inspired) ──────────────────────────────────────────
static constexpr float GRAVITY_UP    = 1500.0f; // px/s² while ascending
static constexpr float GRAVITY_DOWN  = 3000.0f; // px/s² while descending
static constexpr float JUMP_VELOCITY = -650.0f;
static constexpr float JUMP_CUT_VY  = 300.0f;
static constexpr float COYOTE_TIME      = 0.10f;
static constexpr float JUMP_BUFFER_TIME = 0.12f;
static constexpr float PEAK_THRESHOLD   = 100.0f;

static inline int to_tile(float px) { return (int)floorf(px / (float)TILE_SIZE); }

static Rectangle frame_rect(int col, int row)
{
    return Rectangle{ (float)(col * 32), (float)(row * 32), 32.0f, 32.0f };
}

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
    // Align the bottom-left of the hitbox with start_pos.
    player->position = {
        start_pos.x - HITBOX_OFFSET_X,
        start_pos.y - HITBOX_OFFSET_Y - HITBOX_H
    };
    player->velocity_y        = 0.0f;
    player->grounded          = false;
    player->attacking         = false;
    player->landing           = false;
    player->coyote_timer      = 0.0f;
    player->jump_buffer_timer = 0.0f;
    player->double_jumped     = false;
    player->state             = PLAYER_FALLING;

    player->spritesheet = LoadTexture(assets_path("character.png").c_str());
    Texture2D *tex = &player->spritesheet;

    player->anim_idle.texture        = tex;
    player->anim_idle.frame_count    = 4;
    player->anim_idle.frame_duration = 0.15f;
    player->anim_idle.loops          = true;
    player->anim_idle.frames[0]      = frame_rect(0, 0);
    player->anim_idle.frames[1]      = frame_rect(1, 0);
    player->anim_idle.frames[2]      = frame_rect(0, 1);
    player->anim_idle.frames[3]      = frame_rect(1, 1);

    player->anim_walk.texture        = tex;
    player->anim_walk.frame_count    = 4;
    player->anim_walk.frame_duration = 0.1f;
    player->anim_walk.loops          = true;
    for (int i = 0; i < 4; i++)
        player->anim_walk.frames[i] = frame_rect(i, 2);

    player->anim_run.texture        = tex;
    player->anim_run.frame_count    = 8;
    player->anim_run.frame_duration = 0.08f;
    player->anim_run.loops          = true;
    for (int i = 0; i < 8; i++)
        player->anim_run.frames[i] = frame_rect(i, 3);

    anim_init(&player->anim_jump_begin, tex, 5, 0, 4, 0.07f);
    anim_init(&player->anim_jump_peak,  tex, 5, 4, 1, 0.20f);
    anim_init(&player->anim_jump_fall,  tex, 5, 5, 1, 0.15f);
    anim_init(&player->anim_jump_land,  tex, 5, 6, 2, 0.05f);
    anim_init(&player->anim_attack,     tex, 8, 0, 8, 0.05f);

    animation_player_set(&player->anim_player, &player->anim_jump_fall);
}

// ── Collision helpers ─────────────────────────────────────────────────────────
// Returns the four corners of the player hitbox in world pixels.
static void hitbox_corners(const Player *player,
                            float &left, float &right, float &top, float &bottom)
{
    left   = player->position.x + HITBOX_OFFSET_X;
    right  = left + HITBOX_W;
    top    = player->position.y + HITBOX_OFFSET_Y;
    bottom = top + HITBOX_H;
}

// Resolve vertical collision after moving Y.
// Checks the two foot-corners (falling) or the two head-corners (rising).
static void resolve_vertical(Player *player, const Tilemap *tm)
{
    float left, right, top, bottom;
    hitbox_corners(player, left, right, top, bottom);

    if (player->velocity_y >= 0.0f)
    {
        // Falling: check feet
        int ty = to_tile(bottom);
        // Sample slightly inside left/right so we don't catch the tile edge
        if (tilemap_is_solid(tm, to_tile(left  + 1.0f), ty) ||
            tilemap_is_solid(tm, to_tile(right - 1.0f), ty))
        {
            // Snap feet to tile top
            player->position.y = ty * TILE_SIZE - HITBOX_H - HITBOX_OFFSET_Y;
            player->velocity_y = 0.0f;
            player->grounded   = true;
        }
    }
    else
    {
        // Rising: check head
        int ty = to_tile(top);
        if (tilemap_is_solid(tm, to_tile(left  + 1.0f), ty) ||
            tilemap_is_solid(tm, to_tile(right - 1.0f), ty))
        {
            // Snap head to tile bottom
            player->position.y = (ty + 1) * TILE_SIZE - HITBOX_OFFSET_Y;
            player->velocity_y = 0.0f;
        }
    }
}

// Resolve horizontal collision after moving X.
static void resolve_horizontal(Player *player, const Tilemap *tm, float dir_x)
{
    float left, right, top, bottom;
    hitbox_corners(player, left, right, top, bottom);

    // Sample slightly inside top/bottom to avoid catching on floor/ceiling edges
    int ty_top = to_tile(top    + 2.0f);
    int ty_bot = to_tile(bottom - 2.0f);

    if (dir_x > 0.0f)
    {
        int tx = to_tile(right);
        if (tilemap_is_solid(tm, tx, ty_top) || tilemap_is_solid(tm, tx, ty_bot))
            player->position.x = tx * TILE_SIZE - HITBOX_W - HITBOX_OFFSET_X;
    }
    else if (dir_x < 0.0f)
    {
        int tx = to_tile(left);
        if (tilemap_is_solid(tm, tx, ty_top) || tilemap_is_solid(tm, tx, ty_bot))
            player->position.x = (tx + 1) * TILE_SIZE - HITBOX_OFFSET_X;
    }
}

// ─────────────────────────────────────────────────────────────────────────────

void player_update(Player *player, const Tilemap *tm, float dt,
                   PlayerSoundTriggers *triggers, bool input_blocked)
{
    *triggers = PlayerSoundTriggers{};

    PlayerState state_before = player->state;

    bool was_in_air = (player->state == PLAYER_JUMPING ||
                       player->state == PLAYER_PEAK    ||
                       player->state == PLAYER_FALLING);

    // ── Input ─────────────────────────────────────────────────────────
    bool kb_left        = !input_blocked && (IsKeyDown(KEY_LEFT)       || IsKeyDown(KEY_A));
    bool kb_right       = !input_blocked && (IsKeyDown(KEY_RIGHT)      || IsKeyDown(KEY_D));
    bool kb_run         = !input_blocked && (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
    bool jump_pressed   = !input_blocked && IsKeyPressed(KEY_SPACE);
    bool jump_held      = !input_blocked && IsKeyDown(KEY_SPACE);
    bool attack_pressed = !input_blocked && (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsKeyPressed(KEY_X));

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

    // ── Variable jump cut ─────────────────────────────────────────────
    if (!player->grounded && !jump_held && player->velocity_y < -JUMP_CUT_VY)
        player->velocity_y = -JUMP_CUT_VY;

    // ── Gravity and vertical movement ─────────────────────────────────
    bool prev_grounded = player->grounded;
    player->grounded   = false;

    if (prev_grounded && player->velocity_y >= 0.0f)
    {
        // Was on the ground and not jumping: zero accumulated velocity and probe
        // downward by a fixed amount so resolve_vertical snaps back to the exact
        // tile boundary. This eliminates floating-point drift from accumulated
        // grav*dt increments over many frames.
        player->velocity_y  = 0.0f;
        player->position.y += 4.0f; // small constant probe — always < TILE_SIZE
    }
    else
    {
        float grav = (player->velocity_y < 0.0f) ? GRAVITY_UP : GRAVITY_DOWN;
        player->velocity_y += grav * dt;
        player->position.y += player->velocity_y * dt;
    }

    resolve_vertical(player, tm);

    // ── Horizontal movement ───────────────────────────────────────────
    float speed = run ? RUN_SPEED : WALK_SPEED;
    player->velocity_x = dir_x * speed;
    player->position.x += player->velocity_x * dt;
    resolve_horizontal(player, tm, dir_x);

    // ── Coyote time ───────────────────────────────────────────────────
    if (player->grounded)
    {
        player->coyote_timer  = COYOTE_TIME;
        player->double_jumped = false; // reset on landing
    }
    else
        player->coyote_timer = fmaxf(0.0f, player->coyote_timer - dt);

    // ── Jump buffer ───────────────────────────────────────────────────
    if (jump_pressed)
        player->jump_buffer_timer = JUMP_BUFFER_TIME;
    else
        player->jump_buffer_timer = fmaxf(0.0f, player->jump_buffer_timer - dt);

    // ── Execute jump (first jump) ─────────────────────────────────────
    if (player->jump_buffer_timer > 0.0f && player->coyote_timer > 0.0f)
    {
        player->velocity_y        = JUMP_VELOCITY;
        player->grounded          = false;
        player->coyote_timer      = 0.0f;
        player->jump_buffer_timer = 0.0f;
        player->landing           = false;
    }
    // ── Double jump (air jump) ────────────────────────────────────────
    // Available once while airborne — consumed immediately on press (no buffer).
    else if (jump_pressed && !player->grounded && !player->double_jumped)
    {
        player->velocity_y    = JUMP_VELOCITY;
        player->double_jumped = true;
        player->landing       = false;
        // Restart the jump_begin animation from frame 0 for visual feedback.
        player->anim_player.current     = nullptr; // force animation_player_set to reset
        animation_player_set(&player->anim_player, &player->anim_jump_begin);
    }

    // ── Advance animation (before state machine to detect anim end) ───
    int frame_before = player->anim_player.frame_index;
    animation_player_update(&player->anim_player, dt);
    int frame_after  = player->anim_player.frame_index;

    // ── Footstep sound triggers ───────────────────────────────────────
    // Fire when the animation crosses a footstep frame (frame changed AND landed on it).
    // Walk footstep frames: 0, 2  |  Run footstep frames: 0, 2, 4, 6
    if (frame_after != frame_before)
    {
        const Animation *cur = player->anim_player.current;
        if (cur == &player->anim_walk)
        {
            if (frame_after == 0 || frame_after == 2)
                triggers->footstep_walk = true;
        }
        else if (cur == &player->anim_run)
        {
            if (frame_after == 0 || frame_after == 2 || frame_after == 4 || frame_after == 6)
                triggers->footstep_run = true;
        }
    }
    player->prev_anim_frame = frame_after;

    // ── Landing trigger ───────────────────────────────────────────────
    if (player->grounded && was_in_air && !player->attacking)
    {
        player->landing   = true;
        triggers->landed  = true;
    }

    if (player->landing && !player->grounded)
        player->landing = false;

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
        player->attacking  = true;
        player->landing    = false;
        triggers->attacked = true;
        animation_player_set(&player->anim_player, &player->anim_attack);
    }

    // ── Movement state ────────────────────────────────────────────────
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

    // ── Fire footstep immediately on state entry ──────────────────────
    // Avoids the first-frame delay caused by waiting for a frame transition.
    if (player->state != state_before)
    {
        if (player->state == PLAYER_WALKING) triggers->footstep_walk = true;
        if (player->state == PLAYER_RUNNING) triggers->footstep_run  = true;
    }

    player->prev_state = state_before;

    // ── Flip sprite ───────────────────────────────────────────────────
    if (dir_x < 0.0f) player->anim_player.flip_h = true;
    if (dir_x > 0.0f) player->anim_player.flip_h = false;
}

void player_draw(const Player *player)
{
    animation_player_draw(&player->anim_player, player->position, SPRITE_SCALE);
}

void player_cleanup(Player *player)
{
    UnloadTexture(player->spritesheet);
}

Vector2 player_center(const Player *player)
{
    return {
        player->position.x + HITBOX_OFFSET_X + HITBOX_W * 0.5f,
        player->position.y + HITBOX_OFFSET_Y + HITBOX_H * 0.5f
    };
}
