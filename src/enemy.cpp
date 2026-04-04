#include "enemy.h"
#include "assault_rifle.h"
#include "deagle.h"
#include "game.h"
#include "player.h"   // FacingDirection, SPRITE_SCALE constants
#include "raymath.h"
#include <cmath>
#include <cstdlib>

// ── Sprite / animation constants ──────────────────────────────────────────────
static constexpr int   ENEMY_FRAME_W      = 16;
static constexpr int   ENEMY_FRAME_H      = 32;
static constexpr float ENEMY_SPRITE_SCALE = 3.0f;
static constexpr int   IDLE_FRAME_COUNT   = 4;
static constexpr int   RUN_FRAME_COUNT    = 6;
static const Color     ENEMY_TINT         = { 160, 30, 50, 255 };

// ── AI tuning ─────────────────────────────────────────────────────────────────
static constexpr float VISION_RANGE       = 250.0f;
static constexpr float VISION_HALF_ANGLE  = 45.0f;   // degrees, half of 90° cone
static constexpr float ALERT_DELAY_MIN    = 0.3f;
static constexpr float ALERT_DELAY_MAX    = 0.5f;
static constexpr float CHASE_SPEED        = 185.0f;
static constexpr float ATTACK_RANGE       = 200.0f;
static constexpr float EXTRA_SPREAD_DEG   = 12.0f;   // extra inaccuracy per shot

// Weapon orbit rendering — mirrors weapon_manager constants.
static constexpr float ORBIT_DIST     = 22.0f;
static constexpr float SIDE_OFFSET    = 6.0f;
static constexpr float ROT_LERP_SPEED = 18.0f;
static constexpr float RECOIL_DECAY   = 12.0f;

// ── Helpers ───────────────────────────────────────────────────────────────────

static float randf(float lo, float hi)
{
    return lo + (hi - lo) * ((float)std::rand() / (float)RAND_MAX);
}

static float lerp_angle(float cur, float target, float speed, float dt)
{
    float diff = target - cur;
    while (diff >  180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    return cur + diff * std::min(speed * dt, 1.0f);
}

static Rectangle frame_rect(int col, int row)
{
    return Rectangle{
        (float)(col * ENEMY_FRAME_W),
        (float)(row * ENEMY_FRAME_H),
        (float)ENEMY_FRAME_W,
        (float)ENEMY_FRAME_H
    };
}

static void init_anim(Animation *a, Texture2D *tex, int row, int frame_count,
                      float frame_dur)
{
    a->texture        = tex;
    a->frame_count    = frame_count;
    a->frame_duration = frame_dur;
    a->loops          = true;
    for (int i = 0; i < frame_count; ++i)
        a->frames[i] = frame_rect(i, row);
}

// ── Directional animation selection (mirrors player.cpp logic) ────────────────

struct DirRow { int row; bool flip_h; };

static FacingDirection angle_to_facing(float angle_deg)
{
    float a = angle_deg;
    if (a < 0.0f) a += 360.0f;
    if (a >= 337.5f || a < 22.5f)  return FACE_RIGHT;
    if (a < 67.5f)                  return FACE_FRONT_RIGHT;
    if (a < 112.5f)                 return FACE_FRONT;
    if (a < 157.5f)                 return FACE_FRONT_LEFT;
    if (a < 202.5f)                 return FACE_LEFT;
    if (a < 247.5f)                 return FACE_BACK_LEFT;
    if (a < 292.5f)                 return FACE_BACK;
    return FACE_BACK_RIGHT;
}

static DirRow facing_to_dir_row(FacingDirection fd)
{
    switch (fd)
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

// ── Vision ────────────────────────────────────────────────────────────────────

static bool has_line_of_sight(const Tilemap *tm, Vector2 from, Vector2 to)
{
    if (!tm) return true;
    Vector2 delta = Vector2Subtract(to, from);
    float dist = Vector2Length(delta);
    if (dist < 1.0f) return true;
    Vector2 step = Vector2Scale(Vector2Normalize(delta), (float)TILE_SIZE * 0.5f);
    int steps = (int)(dist / ((float)TILE_SIZE * 0.5f));
    Vector2 cur = from;
    for (int i = 0; i < steps; ++i)
    {
        cur = Vector2Add(cur, step);
        int tx = (int)floorf(cur.x / (float)TILE_SIZE);
        int ty = (int)floorf(cur.y / (float)TILE_SIZE);
        if (tilemap_is_solid(tm, tx, ty)) return false;
    }
    return true;
}

static bool can_see_player(const Enemy& e, Vector2 player_pos, const Tilemap *tm)
{
    Vector2 to_p = Vector2Subtract(player_pos, e.position);
    float dist = Vector2Length(to_p);
    if (dist > VISION_RANGE) return false;

    float angle_to = atan2f(to_p.y, to_p.x) * RAD2DEG;
    float diff = angle_to - e.facing_angle;
    while (diff >  180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    if (fabsf(diff) > VISION_HALF_ANGLE) return false;

    return has_line_of_sight(tm, e.position, player_pos);
}

// ── Movement ──────────────────────────────────────────────────────────────────

static Vector2 move_with_slide(Vector2 pos, Vector2 delta, const Tilemap *tm)
{
    if (!tm) return Vector2Add(pos, delta);
    auto solid = [&](Vector2 p) {
        int tx = (int)floorf(p.x / (float)TILE_SIZE);
        int ty = (int)floorf(p.y / (float)TILE_SIZE);
        return tilemap_is_solid(tm, tx, ty);
    };
    Vector2 full = Vector2Add(pos, delta);
    if (!solid(full)) return full;
    Vector2 sx = Vector2Add(pos, { delta.x, 0.0f });
    if (!solid(sx)) return sx;
    Vector2 sy = Vector2Add(pos, { 0.0f, delta.y });
    if (!solid(sy)) return sy;
    return pos;
}

// ── Weapon helpers ────────────────────────────────────────────────────────────

static Vector2 calc_muzzle(const Enemy& e)
{
    if (!e.weapon) return e.position;
    float dist = ORBIT_DIST + (float)e.weapon->sprite_width() * e.weapon->render_scale();
    return Vector2Add(e.position, Vector2Scale(e.aim_dir, dist));
}

// ── Public API ────────────────────────────────────────────────────────────────

void enemies_init(EnemyManager *em)
{
    em->sprite_sheet = LoadTexture(assets_path("character/idle.png").c_str());
    em->run_sheet    = LoadTexture(assets_path("character/run.png").c_str());

    for (int row = 0; row < EnemyManager::ANIM_ROW_COUNT; ++row)
    {
        init_anim(&em->anim_idle_rows[row], &em->sprite_sheet, row, IDLE_FRAME_COUNT, 0.18f);
        init_anim(&em->anim_run_rows [row], &em->run_sheet,    row, RUN_FRAME_COUNT,  0.10f);
    }

    em->enemies.clear();
}

void enemies_load_from_tilemap(EnemyManager *em, const Tilemap *tm)
{
    em->enemies.clear();
    if (!tm) return;

    for (const auto& obj : tm->objects)
    {
        if (obj.name != "enemy") continue;

        Enemy e;
        e.position = { obj.x, obj.y };
        e.alive    = true;

        std::string facing = "down";
        auto it = obj.properties.find("facing");
        if (it != obj.properties.end()) facing = it->second;

        // Convert facing string to angle.
        if      (facing == "right") e.facing_angle = 0.0f;
        else if (facing == "down")  e.facing_angle = 90.0f;
        else if (facing == "left")  e.facing_angle = 180.0f;
        else if (facing == "up")    e.facing_angle = 270.0f;
        else                        e.facing_angle = 90.0f;

        e.aim_dir = { cosf(e.facing_angle * DEG2RAD), sinf(e.facing_angle * DEG2RAD) };

        // Random weapon (50/50).
        if (std::rand() % 2 == 0)
            e.weapon = std::make_unique<AssaultRifle>();
        else
            e.weapon = std::make_unique<Deagle>();

        e.weapon->current_ammo     = e.weapon->magazine_size();
        e.weapon->reserve_ammo     = e.weapon->initial_reserve();
        e.weapon_render_rotation   = e.facing_angle;

        // Set starting animation from spawn facing.
        FacingDirection fd = angle_to_facing(e.facing_angle);
        DirRow          dr = facing_to_dir_row(fd);
        animation_player_set(&e.anim_player, &em->anim_idle_rows[dr.row]);
        e.anim_player.flip_h = dr.flip_h;

        em->enemies.push_back(std::move(e));
    }
}

void enemies_clear(EnemyManager *em)
{
    em->enemies.clear();
}

void enemies_update(EnemyManager *em, BulletSystem *bullets, AudioState *audio,
                    const Tilemap *tm, Vector2 player_center, float dt)
{
    for (auto& e : em->enemies)
    {
        if (!e.alive) continue;

        // ── State machine ─────────────────────────────────────────────
        bool is_moving = false;

        switch (e.ai_state)
        {
        case EnemyAIState::IDLE:
            // Only vision check happens in IDLE — once triggered, never returns.
            if (can_see_player(e, player_center, tm))
            {
                e.ai_state   = EnemyAIState::ALERT;
                e.alert_timer = randf(ALERT_DELAY_MIN, ALERT_DELAY_MAX);
            }
            break;

        case EnemyAIState::ALERT:
            // Track player during reaction delay (enemy has spotted them).
            {
                Vector2 to_p = Vector2Subtract(player_center, e.position);
                if (Vector2LengthSqr(to_p) > 0.01f)
                {
                    e.aim_dir     = Vector2Normalize(to_p);
                    e.facing_angle = atan2f(e.aim_dir.y, e.aim_dir.x) * RAD2DEG;
                }
            }
            e.alert_timer -= dt;
            if (e.alert_timer <= 0.0f)
            {
                float dist = Vector2Distance(e.position, player_center);
                e.ai_state = (dist <= ATTACK_RANGE) ? EnemyAIState::ATTACK : EnemyAIState::CHASE;
            }
            break;

        case EnemyAIState::CHASE:
            // Always know where the player is — no lost-sight logic.
            {
                Vector2 to_p = Vector2Subtract(player_center, e.position);
                float dist = Vector2Length(to_p);
                if (dist > 0.01f)
                {
                    e.aim_dir      = Vector2Scale(to_p, 1.0f / dist);
                    e.facing_angle = atan2f(e.aim_dir.y, e.aim_dir.x) * RAD2DEG;
                }
                if (dist <= ATTACK_RANGE)
                {
                    e.ai_state = EnemyAIState::ATTACK;
                }
                else
                {
                    Vector2 delta = Vector2Scale(e.aim_dir, CHASE_SPEED * dt);
                    e.position = move_with_slide(e.position, delta, tm);
                    is_moving  = true;
                }

                // Fire while chasing — same continuous logic as ATTACK state.
                e.fire_cooldown -= dt;
                if (e.fire_cooldown <= 0.0f && e.weapon)
                {
                    float base_ang   = atan2f(e.aim_dir.y, e.aim_dir.x);
                    float extra      = randf(-EXTRA_SPREAD_DEG, EXTRA_SPREAD_DEG) * DEG2RAD;
                    Vector2 fire_dir = { cosf(base_ang + extra), sinf(base_ang + extra) };

                    e.weapon->fire(bullets, calc_muzzle(e), fire_dir, BulletOwner::ENEMY);
                    e.weapon->play_shot_sound(audio->master_volume, audio->sfx_volume);
                    e.weapon_recoil_offset += e.weapon->recoil_distance();

                    e.weapon->current_ammo--;
                    if (e.weapon->current_ammo <= 0)
                        e.weapon->current_ammo = e.weapon->magazine_size();

                    e.fire_cooldown = (e.weapon->fire_rate() > 0.0f)
                        ? 1.0f / e.weapon->fire_rate()
                        : 0.15f;
                }
            }
            break;

        case EnemyAIState::ATTACK:
            // Always know where the player is — no lost-sight logic.
            {
                Vector2 to_p = Vector2Subtract(player_center, e.position);
                float dist = Vector2Length(to_p);
                if (dist > 0.01f)
                {
                    e.aim_dir     = Vector2Scale(to_p, 1.0f / dist);
                    e.facing_angle = atan2f(e.aim_dir.y, e.aim_dir.x) * RAD2DEG;
                }
                if (dist > ATTACK_RANGE * 1.2f)
                {
                    e.ai_state = EnemyAIState::CHASE;
                    break;
                }

                // Continuous fire on weapon cooldown — same pattern as player auto-fire.
                e.fire_cooldown -= dt;
                if (e.fire_cooldown <= 0.0f && e.weapon)
                {
                    float base_ang = atan2f(e.aim_dir.y, e.aim_dir.x);
                    float extra    = randf(-EXTRA_SPREAD_DEG, EXTRA_SPREAD_DEG) * DEG2RAD;
                    Vector2 fire_dir = { cosf(base_ang + extra), sinf(base_ang + extra) };

                    e.weapon->fire(bullets, calc_muzzle(e), fire_dir, BulletOwner::ENEMY);
                    e.weapon->play_shot_sound(audio->master_volume, audio->sfx_volume);
                    e.weapon_recoil_offset += e.weapon->recoil_distance();

                    e.weapon->current_ammo--;
                    if (e.weapon->current_ammo <= 0)
                        e.weapon->current_ammo = e.weapon->magazine_size(); // instant reload

                    e.fire_cooldown = (e.weapon->fire_rate() > 0.0f)
                        ? 1.0f / e.weapon->fire_rate()
                        : 0.15f;
                }
            }
            break;

        case EnemyAIState::DEAD:
            break;
        }

        // ── Directional animation ─────────────────────────────────────
        FacingDirection fd = angle_to_facing(e.facing_angle);
        DirRow          dr = facing_to_dir_row(fd);
        const Animation *next = is_moving
            ? &em->anim_run_rows [dr.row]
            : &em->anim_idle_rows[dr.row];
        animation_player_set(&e.anim_player, next);
        e.anim_player.flip_h = dr.flip_h;
        animation_player_update(&e.anim_player, dt);

        // ── Weapon orbit render state ─────────────────────────────────
        if (e.weapon)
        {
            float target_rot = atan2f(e.aim_dir.y, e.aim_dir.x) * RAD2DEG;
            e.weapon_render_rotation = lerp_angle(
                e.weapon_render_rotation, target_rot, ROT_LERP_SPEED, dt);

            float r    = e.weapon_render_rotation * DEG2RAD;
            Vector2 perp = { -e.aim_dir.y, e.aim_dir.x };
            e.weapon_render_pos = {
                e.position.x + cosf(r) * ORBIT_DIST + perp.x * SIDE_OFFSET
                    - e.aim_dir.x * e.weapon_recoil_offset,
                e.position.y + sinf(r) * ORBIT_DIST + perp.y * SIDE_OFFSET
                    - e.aim_dir.y * e.weapon_recoil_offset
            };
            e.weapon_recoil_offset -= e.weapon_recoil_offset * RECOIL_DECAY * dt;
            if (e.weapon_recoil_offset < 0.05f) e.weapon_recoil_offset = 0.0f;
        }
    }
}

void enemies_draw(const EnemyManager *em)
{
    for (const auto& e : em->enemies)
    {
        if (!e.alive) continue;

        // ── Weapon ────────────────────────────────────────────────────
        if (e.weapon)
        {
            float sw = (float)e.weapon->sprite_width()  * e.weapon->render_scale();
            float sh = (float)e.weapon->sprite_height() * e.weapon->render_scale();

            float norm_rot = fmodf(e.weapon_render_rotation, 360.0f);
            if (norm_rot < 0.0f) norm_rot += 360.0f;
            bool flip = (norm_rot >= 90.0f && norm_rot <= 270.0f);

            Rectangle src = { 0, 0,
                (float)e.weapon->sprite_width(),
                flip ? -(float)e.weapon->sprite_height() : (float)e.weapon->sprite_height() };
            Rectangle dst = { e.weapon_render_pos.x, e.weapon_render_pos.y - sh * 0.5f, sw, sh };
            DrawTexturePro(e.weapon->texture(), src, dst, { 0.0f, sh * 0.5f },
                           e.weapon_render_rotation, WHITE);
        }

        // ── Sprite (animated, with enemy tint) ───────────────────────
        if (!e.anim_player.current) continue;

        Rectangle source = e.anim_player.current->frames[e.anim_player.frame_index];
        if (e.anim_player.flip_h) source.width = -source.width;

        float fw = (float)ENEMY_FRAME_W * ENEMY_SPRITE_SCALE;
        float fh = (float)ENEMY_FRAME_H * ENEMY_SPRITE_SCALE;
        Rectangle dst = {
            e.position.x - fw * 0.5f,
            e.position.y - fh * 0.5f,
            fw, fh
        };
        DrawTexturePro(*e.anim_player.current->texture, source, dst,
                       { 0.0f, 0.0f }, 0.0f, ENEMY_TINT);
    }
}

Rectangle enemy_hitbox_rect(const Enemy *enemy)
{
    return Rectangle{
        enemy->position.x - ENEMY_HITBOX_W * 0.5f,
        enemy->position.y - ENEMY_HITBOX_H * 0.5f,
        ENEMY_HITBOX_W,
        ENEMY_HITBOX_H
    };
}

void enemies_cleanup(EnemyManager *em)
{
    if (em->sprite_sheet.id != 0) UnloadTexture(em->sprite_sheet);
    if (em->run_sheet.id     != 0) UnloadTexture(em->run_sheet);
    em->sprite_sheet = Texture2D{};
    em->run_sheet    = Texture2D{};
    em->enemies.clear(); // unique_ptr weapons destroyed here
}
