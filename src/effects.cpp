#include "effects.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

// ── Constants ─────────────────────────────────────────────────────────────────

// Splatter burst (immediate on hit)
static constexpr int   SPLATTER_COUNT_MIN    = 40;
static constexpr int   SPLATTER_COUNT_MAX    = 80;
static constexpr float SPLATTER_SPEED_MIN    = 300.0f;
static constexpr float SPLATTER_SPEED_MAX    = 600.0f;
static constexpr float SPLATTER_CONE_HALF    = 0.70f;  // half-angle in radians (~40°) for forward cone
// 60% of pixels go into the forward cone, 40% scatter fully random.
static constexpr float SPLATTER_FORWARD_FRAC = 0.60f;

// Ooze sources (slow pooling trickle after the burst)
static constexpr int   OOZE_SRC_COUNT_MIN   = 5;
static constexpr int   OOZE_SRC_COUNT_MAX   = 10;
static constexpr float OOZE_DURATION_MIN    = 1.0f;   // seconds each source emits
static constexpr float OOZE_DURATION_MAX    = 2.0f;
static constexpr float OOZE_INTERVAL_MIN    = 0.10f;  // seconds between emissions
static constexpr float OOZE_INTERVAL_MAX    = 0.20f;
static constexpr float OOZE_SPEED_MIN       = 10.0f;
static constexpr float OOZE_SPEED_MAX       = 30.0f;
static constexpr float OOZE_SCATTER         = 6.0f;   // spawn position jitter radius

// Physics
// Drag applied as powf(DRAG_BASE, dt * 60) — frame-rate independent.
// At 60 fps this is exactly DRAG_BASE per frame; at any other rate the
// deceleration curve in real-time is identical.
static constexpr float DRAG_BASE            = 0.90f;
static constexpr float SETTLE_SPEED         = 10.0f;  // px/sec — below this the pixel stops

// Active pixel cap (settled stains have no cap).
static constexpr int   PIXEL_CAP            = 500;

// ── Blood colour palette ──────────────────────────────────────────────────────

static const Color BLOOD_PALETTE[] = {
    { 139,  0,  0, 255 }, // dark crimson
    { 200,  0,  0, 255 }, // bright red
    { 100,  0,  0, 255 }, // deep maroon
    {  60,  0,  0, 255 }, // near-black
};
static constexpr int PALETTE_SIZE = 4;

// ── Ooze source (module-private — not exposed in EffectsSystem) ───────────────

struct OozeSource {
    Vector2 position{};
    float   age           = 0.0f;
    float   duration      = 1.5f;
    float   emit_timer    = 0.0f;
    float   emit_interval = 0.15f;
};

static std::vector<OozeSource> s_ooze_sources;

// ── Helpers ───────────────────────────────────────────────────────────────────

static float randf(float lo, float hi)
{
    return lo + (hi - lo) * ((float)std::rand() / (float)RAND_MAX);
}

static int randi(int lo, int hi)
{
    return lo + std::rand() % (hi - lo + 1);
}

static int random_pixel_size()
{
    // Weighted: 1px = 50%, 2px = 33%, 3px = 17%
    int r = randi(0, 5);
    if (r < 3) return 1;
    if (r < 5) return 2;
    return 3;
}

static Color random_blood_color()
{
    Color base = BLOOD_PALETTE[randi(0, PALETTE_SIZE - 1)];
    // Per-channel ±15 noise so no two pixels are identical.
    auto vary = [](int v, int d) -> unsigned char {
        int result = v + d;
        return (unsigned char)(result < 0 ? 0 : result > 255 ? 255 : result);
    };
    return Color{
        vary(base.r, randi(-15, 15)),
        vary(base.g, randi(-15, 15)),
        vary(base.b, randi(-15, 15)),
        255
    };
}

// Tile coordinate from world pixel position.
static int to_tile(float px) { return (int)floorf(px / (float)TILE_SIZE); }

static void settle_pixel(EffectsSystem *effects, BloodPixel &p)
{
    p.settled = true;
    SettledPixel s;
    s.position = p.position;
    s.size     = p.size;
    s.color    = p.color;
    effects->stains.push_back(s);
}

static BloodPixel make_pixel(Vector2 origin, Vector2 dir, float speed)
{
    BloodPixel p;
    p.position     = origin;
    p.velocity     = Vector2Scale(dir, speed);
    p.age          = 0.0f;
    p.max_lifetime = 2.0f;
    p.size         = random_pixel_size();
    p.color        = random_blood_color();
    p.settled      = false;
    return p;
}

// ── Public API ────────────────────────────────────────────────────────────────

void effects_spawn_blood(EffectsSystem *effects, Vector2 position, Vector2 bullet_direction)
{
    Vector2 fwd = (Vector2LengthSqr(bullet_direction) > 0.0001f)
        ? Vector2Normalize(bullet_direction)
        : Vector2{ 0.0f, 1.0f };

    float fwd_angle = atan2f(fwd.y, fwd.x);
    int   count     = randi(SPLATTER_COUNT_MIN, SPLATTER_COUNT_MAX);
    int   forward_n = (int)(count * SPLATTER_FORWARD_FRAC);

    for (int i = 0; i < count; ++i)
    {
        Vector2 dir;
        if (i < forward_n)
        {
            // Forward cone: ±SPLATTER_CONE_HALF radians around bullet direction.
            float angle = fwd_angle + randf(-SPLATTER_CONE_HALF, SPLATTER_CONE_HALF);
            dir = { cosf(angle), sinf(angle) };
        }
        else
        {
            // Full random scatter for the remaining 40%.
            float angle = randf(0.0f, 2.0f * 3.14159265f);
            dir = { cosf(angle), sinf(angle) };
        }

        if ((int)effects->pixels.size() >= PIXEL_CAP) break;
        effects->pixels.push_back(make_pixel(position, dir,
                                             randf(SPLATTER_SPEED_MIN, SPLATTER_SPEED_MAX)));
    }

    // Register ooze sources for the slow post-burst pooling.
    int src_count = randi(OOZE_SRC_COUNT_MIN, OOZE_SRC_COUNT_MAX);
    for (int i = 0; i < src_count; ++i)
    {
        OozeSource src;
        src.position      = { position.x + randf(-OOZE_SCATTER, OOZE_SCATTER),
                               position.y + randf(-OOZE_SCATTER, OOZE_SCATTER) };
        src.age           = 0.0f;
        src.duration      = randf(OOZE_DURATION_MIN, OOZE_DURATION_MAX);
        src.emit_timer    = 0.0f;
        src.emit_interval = randf(OOZE_INTERVAL_MIN, OOZE_INTERVAL_MAX);
        s_ooze_sources.push_back(src);
    }
}

void effects_update(EffectsSystem *effects, const Tilemap *tm, float dt)
{
    // ── Ooze source emission ───────────────────────────────────────────
    for (auto& src : s_ooze_sources)
    {
        src.age        += dt;
        src.emit_timer += dt;

        if (src.emit_timer >= src.emit_interval)
        {
            src.emit_timer -= src.emit_interval;
            int n = randi(1, 2);
            for (int i = 0; i < n; ++i)
            {
                if ((int)effects->pixels.size() >= PIXEL_CAP) break;
                float angle = randf(0.0f, 2.0f * 3.14159265f);
                Vector2 dir = { cosf(angle), sinf(angle) };
                Vector2 origin = { src.position.x + randf(-2.0f, 2.0f),
                                   src.position.y + randf(-2.0f, 2.0f) };
                effects->pixels.push_back(make_pixel(origin, dir,
                                                     randf(OOZE_SPEED_MIN, OOZE_SPEED_MAX)));
            }
        }
    }

    s_ooze_sources.erase(
        std::remove_if(s_ooze_sources.begin(), s_ooze_sources.end(),
                       [](const OozeSource& s) { return s.age >= s.duration; }),
        s_ooze_sources.end());

    // ── Pixel physics ─────────────────────────────────────────────────
    float drag = (dt > 0.0f) ? powf(DRAG_BASE, dt * 60.0f) : 1.0f;

    for (auto& p : effects->pixels)
    {
        if (p.settled) continue;

        p.age += dt;

        Vector2 new_pos = Vector2Add(p.position, Vector2Scale(p.velocity, dt));
        p.velocity = Vector2Scale(p.velocity, drag);

        // Wall collision: check the center of the pixel's new position.
        bool wall_hit = false;
        if (tm)
        {
            float cx = new_pos.x + p.size * 0.5f;
            float cy = new_pos.y + p.size * 0.5f;
            if (tilemap_is_solid(tm, to_tile(cx), to_tile(cy)))
                wall_hit = true;
        }

        if (!wall_hit)
            p.position = new_pos;

        // Settle if slow enough, hit a wall, or exceeded safety lifetime.
        float speed_sq = Vector2LengthSqr(p.velocity);
        if (speed_sq < SETTLE_SPEED * SETTLE_SPEED || wall_hit || p.age >= p.max_lifetime)
            settle_pixel(effects, p);
    }

    // Move settled pixels to stains; erase from active list.
    effects->pixels.erase(
        std::remove_if(effects->pixels.begin(), effects->pixels.end(),
                       [](const BloodPixel& p) { return p.settled; }),
        effects->pixels.end());
}

void effects_draw_stains(const EffectsSystem *effects)
{
    for (const auto& s : effects->stains)
        DrawRectangle((int)s.position.x, (int)s.position.y, s.size, s.size, s.color);
}

void effects_draw_pixels(const EffectsSystem *effects)
{
    for (const auto& p : effects->pixels)
        DrawRectangle((int)p.position.x, (int)p.position.y, p.size, p.size, p.color);
}

void effects_clear(EffectsSystem *effects)
{
    effects->pixels.clear();
    effects->stains.clear();
    s_ooze_sources.clear();
}

int effects_active_count(const EffectsSystem *effects)
{
    return (int)effects->pixels.size();
}

int effects_stain_count(const EffectsSystem *effects)
{
    return (int)effects->stains.size();
}
