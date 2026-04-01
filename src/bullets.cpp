#include "bullets.h"
#include "game.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

static constexpr float BULLET_SPEED = 900.0f;
static constexpr float BULLET_LIFETIME = 0.9f;
static constexpr float BULLET_FRAME_DURATION = 0.05f;
static constexpr float BULLET_SCALE = 2.0f;
static constexpr float BULLET_GLOW_PULSE_SPEED   = 16.0f;
static constexpr float BULLET_INNER_SCALE_BASE   = 1.55f;
static constexpr float BULLET_INNER_SCALE_PULSE  = 0.12f;
static constexpr float BULLET_OUTER_SCALE_BASE   = 1.95f;
static constexpr float BULLET_OUTER_SCALE_PULSE  = 0.18f;
static constexpr Color BULLET_GLOW_OUTER_COLOR   = { 255, 190,  90,  48 };
static constexpr Color BULLET_GLOW_INNER_COLOR   = { 255, 235, 160,  88 };
static constexpr int BULLET_FRAME_SIZE = 16;
static constexpr int BULLET_FRAME_COUNT = 4;

static void draw_bullet_pass(const BulletSystem *system, const Bullet &bullet, Rectangle source,
                             float scale, Color tint)
{
    Rectangle dest = {
        bullet.position.x,
        bullet.position.y,
        (float)BULLET_FRAME_SIZE * scale,
        (float)BULLET_FRAME_SIZE * scale
    };
    Vector2 origin = { dest.width * 0.5f, dest.height * 0.5f };
    float rotation = atan2f(bullet.velocity.y, bullet.velocity.x) * RAD2DEG;

    DrawTexturePro(system->texture, source, dest, origin, rotation, tint);
}

static Rectangle bullet_frame_rect(int frame_index)
{
    return Rectangle{
        (float)(frame_index * BULLET_FRAME_SIZE),
        0.0f,
        (float)BULLET_FRAME_SIZE,
        (float)BULLET_FRAME_SIZE
    };
}

void bullets_init(BulletSystem *system)
{
    system->texture = LoadTexture(assets_path("character/bullet.png").c_str());
    system->bullets.clear();
}

void bullets_spawn(BulletSystem *system, Vector2 origin, Vector2 direction)
{
    if (!system) return;
    if (Vector2LengthSqr(direction) <= 0.0001f) return;

    Bullet bullet;
    bullet.position = origin;
    bullet.velocity = Vector2Scale(Vector2Normalize(direction), BULLET_SPEED);
    bullet.lifetime = BULLET_LIFETIME;
    system->bullets.push_back(bullet);
}

void bullets_clear(BulletSystem *system)
{
    if (!system) return;
    system->bullets.clear();
}

void bullets_update(BulletSystem *system, float dt)
{
    if (!system) return;

    for (Bullet &bullet : system->bullets)
    {
        bullet.age += dt;
        bullet.position = Vector2Add(bullet.position, Vector2Scale(bullet.velocity, dt));
        bullet.frame_timer += dt;

        while (bullet.frame_timer >= BULLET_FRAME_DURATION)
        {
            bullet.frame_timer -= BULLET_FRAME_DURATION;
            bullet.frame_index = (bullet.frame_index + 1) % BULLET_FRAME_COUNT;
        }
    }

    system->bullets.erase(
        std::remove_if(system->bullets.begin(), system->bullets.end(),
                       [](const Bullet &bullet) { return bullet.dead || bullet.age >= bullet.lifetime; }),
        system->bullets.end());
}

void bullets_draw(const BulletSystem *system)
{
    if (!system || system->texture.id == 0) return;

    for (const Bullet &bullet : system->bullets)
    {
        Rectangle source = bullet_frame_rect(bullet.frame_index);
        float pulse = 0.5f + 0.5f * sinf(bullet.age * BULLET_GLOW_PULSE_SPEED);
        float inner_scale = BULLET_SCALE * (BULLET_INNER_SCALE_BASE + pulse * BULLET_INNER_SCALE_PULSE);
        float outer_scale = BULLET_SCALE * (BULLET_OUTER_SCALE_BASE + pulse * BULLET_OUTER_SCALE_PULSE);

        BeginBlendMode(BLEND_ADDITIVE);
            draw_bullet_pass(system, bullet, source, outer_scale, BULLET_GLOW_OUTER_COLOR);
            draw_bullet_pass(system, bullet, source, inner_scale, BULLET_GLOW_INNER_COLOR);
        EndBlendMode();

        draw_bullet_pass(system, bullet, source, BULLET_SCALE, WHITE);
    }
}

void bullets_cleanup(BulletSystem *system)
{
    if (!system) return;
    if (system->texture.id != 0) UnloadTexture(system->texture);
    system->texture = Texture2D{};
    system->bullets.clear();
}
