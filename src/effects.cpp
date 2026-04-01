#include "effects.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

static constexpr int   BLOOD_COUNT_MIN         = 8;
static constexpr int   BLOOD_COUNT_MAX         = 14;
static constexpr float BLOOD_SPEED_MIN         = 80.0f;
static constexpr float BLOOD_SPEED_MAX         = 210.0f;
static constexpr float BLOOD_LIFETIME_MIN      = 0.22f;
static constexpr float BLOOD_LIFETIME_MAX      = 0.48f;
static constexpr float BLOOD_SIZE_MIN          = 3.0f;
static constexpr float BLOOD_SIZE_MAX          = 5.5f;
static constexpr float BLOOD_DRAG              = 0.88f;  // velocity scale per frame — quick decel
static constexpr float BLOOD_DIRECTION_BIAS    = 0.60f;  // 0=pure random spread, 1=pure bullet dir
static constexpr int   STAIN_COUNT_MIN         = 3;
static constexpr int   STAIN_COUNT_MAX         = 5;
static constexpr float STAIN_SCATTER           = 10.0f;  // max offset radius from impact point
static constexpr float STAIN_RADIUS_MIN        = 2.0f;
static constexpr float STAIN_RADIUS_MAX        = 6.0f;
static constexpr unsigned char STAIN_ALPHA     = 180;

static float randf(float lo, float hi)
{
    return lo + (hi - lo) * ((float)std::rand() / (float)RAND_MAX);
}

static int randi(int lo, int hi)
{
    return lo + std::rand() % (hi - lo + 1);
}

static Color random_blood_color()
{
    return Color{
        (unsigned char)randi(125, 180),
        (unsigned char)randi(0,   22),
        (unsigned char)randi(0,   18),
        255
    };
}

void effects_spawn_blood(EffectsSystem *effects, Vector2 position, Vector2 bullet_direction)
{
    // Normalise direction; fall back to downward if zero.
    Vector2 dir = (Vector2LengthSqr(bullet_direction) > 0.0001f)
        ? Vector2Normalize(bullet_direction)
        : Vector2{ 0.0f, 1.0f };

    int count = randi(BLOOD_COUNT_MIN, BLOOD_COUNT_MAX);
    for (int i = 0; i < count; ++i)
    {
        float angle = randf(0.0f, 2.0f * 3.14159265f);
        Vector2 rand_dir = { cosf(angle), sinf(angle) };

        // Mix bullet direction with random spread.
        Vector2 final_dir = Vector2Normalize(Vector2Add(
            Vector2Scale(dir,      BLOOD_DIRECTION_BIAS),
            Vector2Scale(rand_dir, 1.0f - BLOOD_DIRECTION_BIAS)
        ));

        BloodParticle p;
        p.position = position;
        p.velocity = Vector2Scale(final_dir, randf(BLOOD_SPEED_MIN, BLOOD_SPEED_MAX));
        p.lifetime = randf(BLOOD_LIFETIME_MIN, BLOOD_LIFETIME_MAX);
        p.age      = 0.0f;
        p.size     = randf(BLOOD_SIZE_MIN, BLOOD_SIZE_MAX);
        p.color    = random_blood_color();
        effects->particles.push_back(p);
    }

    // Persistent stains — scattered around the impact point.
    int stain_count = randi(STAIN_COUNT_MIN, STAIN_COUNT_MAX);
    for (int i = 0; i < stain_count; ++i)
    {
        float angle = randf(0.0f, 2.0f * 3.14159265f);
        float dist  = randf(0.0f, STAIN_SCATTER);

        BloodStain s;
        s.position = { position.x + cosf(angle) * dist, position.y + sinf(angle) * dist };
        s.radius   = randf(STAIN_RADIUS_MIN, STAIN_RADIUS_MAX);
        s.color    = random_blood_color();
        s.color.a  = STAIN_ALPHA;
        effects->stains.push_back(s);
    }
}

void effects_update(EffectsSystem *effects, float dt)
{
    for (auto& p : effects->particles)
    {
        p.age     += dt;
        p.position = Vector2Add(p.position, Vector2Scale(p.velocity, dt));
        p.velocity = Vector2Scale(p.velocity, BLOOD_DRAG);
    }

    effects->particles.erase(
        std::remove_if(effects->particles.begin(), effects->particles.end(),
                       [](const BloodParticle& p) { return p.age >= p.lifetime; }),
        effects->particles.end());
}

void effects_draw_stains(const EffectsSystem *effects)
{
    for (const auto& s : effects->stains)
        DrawCircleV(s.position, s.radius, s.color);
}

void effects_draw_particles(const EffectsSystem *effects)
{
    for (const auto& p : effects->particles)
    {
        float t    = p.age / p.lifetime;   // 0 at spawn → 1 at death
        float size = p.size * (1.0f - t);  // shrinks to zero
        if (size < 0.5f) continue;
        DrawCircleV(p.position, size, p.color);
    }
}

void effects_clear(EffectsSystem *effects)
{
    effects->particles.clear();
    effects->stains.clear();
}
