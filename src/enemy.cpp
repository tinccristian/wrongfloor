#include "enemy.h"
#include "assault_rifle.h"
#include "deagle.h"
#include "game.h"
#include "raymath.h"
#include <cmath>
#include <cstdlib>

// ── Sprite constants ──────────────────────────────────────────────────────────
static constexpr int   ENEMY_FRAME_W      = 16;
static constexpr int   ENEMY_FRAME_H      = 32;
static constexpr float ENEMY_SPRITE_SCALE = 3.0f;
static const Color     ENEMY_TINT         = { 160, 30, 50, 255 };

// ── AI tuning ─────────────────────────────────────────────────────────────────
static constexpr float VISION_RANGE       = 250.0f;
static constexpr float VISION_HALF_ANGLE  = 45.0f;  // degrees, half of 90° cone
static constexpr float ALERT_DELAY_MIN    = 0.3f;
static constexpr float ALERT_DELAY_MAX    = 0.5f;
static constexpr float LOST_SIGHT_TIMEOUT = 2.0f;
static constexpr float CHASE_SPEED        = 150.0f; // ~68% of player 220 px/s
static constexpr float ATTACK_RANGE       = 200.0f;
static constexpr float EXTRA_SPREAD_DEG   = 12.0f;  // extra inaccuracy for enemy shots
static constexpr int   BURST_MIN          = 2;
static constexpr int   BURST_MAX          = 4;
static constexpr float BURST_PAUSE_MIN    = 0.5f;
static constexpr float BURST_PAUSE_MAX    = 1.0f;

// Weapon orbit rendering — mirrors weapon_manager constants.
static constexpr float ORBIT_DIST      = 22.0f;
static constexpr float SIDE_OFFSET     = 6.0f;
static constexpr float ROT_LERP_SPEED  = 18.0f;
static constexpr float RECOIL_DECAY    = 12.0f;

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

// Map a facing string to the sprite-sheet row and horizontal flip flag.
static void facing_to_row(const std::string& facing, int *out_row, bool *out_flip)
{
    *out_flip = false;
    if (facing == "up")    { *out_row = 4; return; }
    if (facing == "right") { *out_row = 2; return; }
    if (facing == "left")  { *out_row = 2; *out_flip = true; return; }
    *out_row = 0; // "down" or unrecognised → front-facing
}

// Returns true if there is a clear line of sight from 'from' to 'to' in the tilemap.
// Steps along the ray at half-tile intervals.
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

// Returns true if the enemy can currently see the player.
static bool can_see_player(const Enemy& e, Vector2 player_center, const Tilemap *tm)
{
    Vector2 to_player = Vector2Subtract(player_center, e.position);
    float dist = Vector2Length(to_player);
    if (dist > VISION_RANGE) return false;

    // Angle check: compare to facing_angle.
    float angle_to_player = atan2f(to_player.y, to_player.x) * RAD2DEG;
    float diff = angle_to_player - e.facing_angle;
    while (diff >  180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    if (fabsf(diff) > VISION_HALF_ANGLE) return false;

    return has_line_of_sight(tm, e.position, player_center);
}

// Slide-along-wall movement: try full move, then axis components separately.
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

    Vector2 slide_x = Vector2Add(pos, { delta.x, 0.0f });
    if (!solid(slide_x)) return slide_x;

    Vector2 slide_y = Vector2Add(pos, { 0.0f, delta.y });
    if (!solid(slide_y)) return slide_y;

    return pos;
}

// Compute muzzle position for the enemy weapon (same math as weapon_manager).
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

        facing_to_row(facing, &e.sprite_row, &e.sprite_flip);

        // Set initial facing_angle from the facing string.
        if      (facing == "right") e.facing_angle = 0.0f;
        else if (facing == "down")  e.facing_angle = 90.0f;
        else if (facing == "left")  e.facing_angle = 180.0f;
        else if (facing == "up")    e.facing_angle = 270.0f;
        else                        e.facing_angle = 90.0f;

        e.aim_dir = { cosf(e.facing_angle * DEG2RAD), sinf(e.facing_angle * DEG2RAD) };

        // Assign random weapon (50/50).
        if (std::rand() % 2 == 0)
            e.weapon = std::make_unique<AssaultRifle>();
        else
            e.weapon = std::make_unique<Deagle>();

        e.weapon->current_ammo = e.weapon->magazine_size();
        e.weapon->reserve_ammo = e.weapon->initial_reserve();
        e.weapon_render_rotation = e.facing_angle;

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

        bool sees_player = can_see_player(e, player_center, tm);

        // ── State machine ─────────────────────────────────────────────
        switch (e.ai_state)
        {
        case EnemyAIState::IDLE:
            if (sees_player)
            {
                e.ai_state   = EnemyAIState::ALERT;
                e.alert_timer = randf(ALERT_DELAY_MIN, ALERT_DELAY_MAX);
            }
            break;

        case EnemyAIState::ALERT:
            // Update facing toward player during reaction delay.
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
                e.lost_sight_timer = 0.0f;
            }
            if (!sees_player)
            {
                // Lost sight during reaction — back to IDLE.
                e.ai_state = EnemyAIState::IDLE;
            }
            break;

        case EnemyAIState::CHASE:
            if (sees_player)
            {
                e.lost_sight_timer = 0.0f;
                Vector2 to_p = Vector2Subtract(player_center, e.position);
                float dist = Vector2Length(to_p);

                if (dist > 0.01f)
                {
                    e.aim_dir     = Vector2Scale(to_p, 1.0f / dist);
                    e.facing_angle = atan2f(e.aim_dir.y, e.aim_dir.x) * RAD2DEG;
                }

                if (dist <= ATTACK_RANGE)
                {
                    e.ai_state = EnemyAIState::ATTACK;
                }
                else
                {
                    // Move toward player with wall slide.
                    Vector2 delta = Vector2Scale(e.aim_dir, CHASE_SPEED * dt);
                    e.position = move_with_slide(e.position, delta, tm);
                }
            }
            else
            {
                e.lost_sight_timer += dt;
                if (e.lost_sight_timer >= LOST_SIGHT_TIMEOUT)
                    e.ai_state = EnemyAIState::IDLE;
            }
            break;

        case EnemyAIState::ATTACK:
            if (sees_player)
            {
                e.lost_sight_timer = 0.0f;
                // Track aim toward player.
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

                // Burst fire logic.
                if (e.burst_remaining > 0)
                {
                    e.fire_cooldown -= dt;
                    if (e.fire_cooldown <= 0.0f && e.weapon)
                    {
                        // Apply extra spread on top of weapon's own spread.
                        float base_ang = atan2f(e.aim_dir.y, e.aim_dir.x);
                        float extra    = (randf(-EXTRA_SPREAD_DEG, EXTRA_SPREAD_DEG)) * DEG2RAD;
                        float ang      = base_ang + extra;
                        Vector2 fire_dir = { cosf(ang), sinf(ang) };

                        Vector2 muzzle = calc_muzzle(e);
                        e.weapon->fire(bullets, muzzle, fire_dir, BulletOwner::ENEMY);
                        e.weapon->play_shot_sound(audio->master_volume, audio->sfx_volume);

                        e.weapon_recoil_offset += e.weapon->recoil_distance();
                        e.weapon->current_ammo--;
                        if (e.weapon->current_ammo <= 0)
                        {
                            // Instant reload for enemies.
                            e.weapon->current_ammo = e.weapon->magazine_size();
                        }

                        e.burst_remaining--;
                        e.fire_cooldown = (e.weapon->fire_rate() > 0.0f)
                            ? 1.0f / e.weapon->fire_rate()
                            : 0.15f;
                    }
                }
                else
                {
                    // Between bursts.
                    e.burst_timer -= dt;
                    if (e.burst_timer <= 0.0f)
                    {
                        e.burst_remaining = BURST_MIN + std::rand() % (BURST_MAX - BURST_MIN + 1);
                        e.burst_timer     = randf(BURST_PAUSE_MIN, BURST_PAUSE_MAX);
                        e.fire_cooldown   = 0.0f;
                    }
                }
            }
            else
            {
                e.lost_sight_timer += dt;
                if (e.lost_sight_timer >= LOST_SIGHT_TIMEOUT)
                    e.ai_state = EnemyAIState::IDLE;
            }
            break;

        case EnemyAIState::DEAD:
            break;
        }

        // ── Weapon orbit render state update ──────────────────────────
        if (e.weapon && e.alive)
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

        // Update sprite flip to match aim direction.
        float norm = fmodf(e.facing_angle, 360.0f);
        if (norm < 0.0f) norm += 360.0f;
        if (e.ai_state != EnemyAIState::IDLE)
            e.sprite_flip = (norm > 90.0f && norm < 270.0f);
    }
}

void enemies_draw(const EnemyManager *em)
{
    if (em->sprite_sheet.id == 0) return;

    float fw = (float)ENEMY_FRAME_W * ENEMY_SPRITE_SCALE;
    float fh = (float)ENEMY_FRAME_H * ENEMY_SPRITE_SCALE;

    for (const auto& e : em->enemies)
    {
        if (!e.alive) continue;

        // Draw weapon behind/in-front based on aim angle (simple: always draw first).
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
            Vector2   org = { 0.0f, sh * 0.5f };
            DrawTexturePro(e.weapon->texture(), src, dst, org, e.weapon_render_rotation, WHITE);
        }

        // Draw enemy sprite.
        Rectangle src = {
            0.0f,
            (float)(e.sprite_row * ENEMY_FRAME_H),
            e.sprite_flip ? -(float)ENEMY_FRAME_W : (float)ENEMY_FRAME_W,
            (float)ENEMY_FRAME_H
        };
        Rectangle dst = {
            e.position.x - fw * 0.5f,
            e.position.y - fh * 0.5f,
            fw, fh
        };
        DrawTexturePro(em->sprite_sheet, src, dst, Vector2{ 0.0f, 0.0f }, 0.0f, ENEMY_TINT);
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
    em->sprite_sheet = Texture2D{};
    em->enemies.clear(); // unique_ptr weapons destroyed here
}
