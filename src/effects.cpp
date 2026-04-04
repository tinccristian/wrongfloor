#include "effects.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

// ── Pixel size ────────────────────────────────────────────────────────────────
// Uniform 3x3 world-space pixels — one "art pixel" in a 32px-tile world.
static constexpr int PIXEL_SIZE = 3;

// ── Initial splatter burst ────────────────────────────────────────────────────
static constexpr int   SPLATTER_COUNT_MIN    = 150;
static constexpr int   SPLATTER_COUNT_MAX    = 250;
static constexpr float SPLATTER_SPEED_MIN    = 500.0f;
static constexpr float SPLATTER_SPEED_MAX    = 900.0f;
// 120° wide fan: 70% go into the cone, 30% scatter randomly for the messy look.
static constexpr float SPLATTER_CONE_HALF    = 1.047f;  // ±60°
static constexpr float SPLATTER_FWD_FRAC     = 0.70f;

// High-velocity "spear" pixels — fly much further, smear on walls.
static constexpr int   SPEAR_COUNT_MIN       = 10;
static constexpr int   SPEAR_COUNT_MAX       = 20;
static constexpr float SPEAR_SPEED_MIN       = 800.0f;
static constexpr float SPEAR_SPEED_MAX       = 1200.0f;
static constexpr float SPEAR_CONE_HALF       = 1.047f;  // same 120° cone

// ── Fountain (sustained spray for first 0.3 s) ────────────────────────────────
static constexpr float FOUNTAIN_DURATION     = 0.30f;
static constexpr float FOUNTAIN_INTERVAL     = 0.020f;  // emit every 20 ms
static constexpr int   FOUNTAIN_EMIT_MIN     = 3;
static constexpr int   FOUNTAIN_EMIT_MAX     = 5;
static constexpr float FOUNTAIN_SPEED_MIN    = 300.0f;
static constexpr float FOUNTAIN_SPEED_MAX    = 700.0f;
static constexpr float FOUNTAIN_CONE_HALF    = 0.785f;  // ±45° — tighter than burst

// ── Ooze sources (pool formation over 2-3 s) ──────────────────────────────────
static constexpr int   OOZE_SRC_COUNT_MIN    = 15;
static constexpr int   OOZE_SRC_COUNT_MAX    = 25;
static constexpr float OOZE_DURATION_MIN     = 2.0f;
static constexpr float OOZE_DURATION_MAX     = 3.0f;
static constexpr float OOZE_INTERVAL_MIN     = 0.050f;  // fast emission for density
static constexpr float OOZE_INTERVAL_MAX     = 0.100f;
static constexpr int   OOZE_EMIT_MIN         = 2;
static constexpr int   OOZE_EMIT_MAX         = 3;
static constexpr float OOZE_SPEED_MIN        = 15.0f;
static constexpr float OOZE_SPEED_MAX        = 55.0f;
static constexpr float OOZE_SCATTER          = 35.0f;   // source jitter radius

// ── Physics ───────────────────────────────────────────────────────────────────
// Frame-rate-independent: velocity *= powf(DRAG_BASE, dt * 60).
// At 60 fps this is exactly DRAG_BASE per frame.
static constexpr float DRAG_BASE             = 0.90f;
static constexpr float SETTLE_SPEED          = 12.0f;   // px/sec — below this, pixel stops

// ── Wall smear ────────────────────────────────────────────────────────────────
static constexpr int   WALL_SMEAR_COUNT      = 3;       // extra settled pixels per spear impact
static constexpr float WALL_SMEAR_SCATTER    = 5.0f;    // px scatter around impact point

// ── Active pixel cap ──────────────────────────────────────────────────────────
static constexpr int   PIXEL_CAP             = 2000;

// ── Colour palettes ───────────────────────────────────────────────────────────

// In-flight splatter: bright and fresh.
static const Color SPLATTER_COLORS[] = {
    { 220, 10, 10, 255 },   // fresh bright red     (most common)
    { 220, 10, 10, 255 },   // duplicate — higher weight
    { 200,  5,  5, 255 },   // slightly dimmer red
    { 180,  0,  0, 255 },   // medium red
    { 230, 50, 50, 255 },   // pinkish highlight    (rare)
};
static constexpr int SPLATTER_COLOR_COUNT = 5;

// Settled stains and ooze: dark pooling blood.
static const Color SETTLE_COLORS[] = {
    { 120,  0,  0, 255 },   // dark crimson
    { 100,  0,  0, 255 },   // darker
    {  80,  0,  0, 255 },   // very dark
    {  50,  0,  0, 255 },   // near-black  (most common)
    {  50,  0,  0, 255 },   // duplicate — higher weight
};
static constexpr int SETTLE_COLOR_COUNT = 5;

// ── Module-private source types ───────────────────────────────────────────────

struct FountainSource {
    Vector2 position{};
    Vector2 direction{};    // bias direction inherited from bullet
    float   age           = 0.0f;
    float   emit_timer    = 0.0f;
};

struct OozeSource {
    Vector2 position{};
    float   age           = 0.0f;
    float   duration      = 2.5f;
    float   emit_timer    = 0.0f;
    float   emit_interval = 0.07f;
};

static std::vector<FountainSource> s_fountains;
static std::vector<OozeSource>     s_ooze_sources;

// ── Helpers ───────────────────────────────────────────────────────────────────

static float randf(float lo, float hi)
{
    return lo + (hi - lo) * ((float)std::rand() / (float)RAND_MAX);
}

static int randi(int lo, int hi)
{
    return lo + std::rand() % (hi - lo + 1);
}

static Color splatter_color()
{
    return SPLATTER_COLORS[randi(0, SPLATTER_COLOR_COUNT - 1)];
}

static Color settle_color()
{
    return SETTLE_COLORS[randi(0, SETTLE_COLOR_COUNT - 1)];
}

static int to_tile(float px) { return (int)floorf(px / (float)TILE_SIZE); }

// Transfer a pixel to the permanent stain list with a darkened pooling colour.
static void settle_pixel(EffectsSystem *effects, BloodPixel &p)
{
    p.settled = true;
    SettledPixel s;
    s.position = p.position;
    s.size     = p.size;
    s.color    = settle_color();
    effects->stains.push_back(s);
}

// Spawn small wall-smear stains around a spear impact point.
static void spawn_wall_smear(EffectsSystem *effects, Vector2 impact)
{
    for (int i = 0; i < WALL_SMEAR_COUNT; ++i)
    {
        SettledPixel s;
        s.position = { impact.x + randf(-WALL_SMEAR_SCATTER, WALL_SMEAR_SCATTER),
                       impact.y + randf(-WALL_SMEAR_SCATTER, WALL_SMEAR_SCATTER) };
        s.size  = PIXEL_SIZE;
        s.color = settle_color();
        effects->stains.push_back(s);
    }
}

static BloodPixel make_pixel(Vector2 origin, Vector2 dir, float speed,
                             Color color, bool is_spear = false)
{
    BloodPixel p;
    p.position     = origin;
    p.velocity     = Vector2Scale(dir, speed);
    p.age          = 0.0f;
    p.max_lifetime = 3.0f;
    p.size         = PIXEL_SIZE;
    p.color        = color;
    p.is_spear     = is_spear;
    p.settled      = false;
    return p;
}

// Emit a pixel if under the cap.  Returns false when the cap is reached.
static bool try_emit(EffectsSystem *effects, Vector2 origin, Vector2 dir,
                     float speed, Color color, bool is_spear = false)
{
    if ((int)effects->pixels.size() >= PIXEL_CAP) return false;
    effects->pixels.push_back(make_pixel(origin, dir, speed, color, is_spear));
    return true;
}

// ── Public API ────────────────────────────────────────────────────────────────

void effects_spawn_blood(EffectsSystem *effects, Vector2 position, Vector2 bullet_direction)
{
    Vector2 fwd = (Vector2LengthSqr(bullet_direction) > 0.0001f)
        ? Vector2Normalize(bullet_direction)
        : Vector2{ 0.0f, 1.0f };

    float fwd_angle = atan2f(fwd.y, fwd.x);
    static constexpr float TWO_PI = 6.2831853f;

    // ── Regular splatter burst ────────────────────────────────────────
    int count    = randi(SPLATTER_COUNT_MIN, SPLATTER_COUNT_MAX);
    int forward_n = (int)(count * SPLATTER_FWD_FRAC);

    for (int i = 0; i < count; ++i)
    {
        float angle = (i < forward_n)
            ? fwd_angle + randf(-SPLATTER_CONE_HALF, SPLATTER_CONE_HALF)
            : randf(0.0f, TWO_PI);
        Vector2 dir = { cosf(angle), sinf(angle) };
        if (!try_emit(effects, position, dir,
                      randf(SPLATTER_SPEED_MIN, SPLATTER_SPEED_MAX), splatter_color()))
            break;
    }

    // ── High-velocity spear pixels ────────────────────────────────────
    int spear_n = randi(SPEAR_COUNT_MIN, SPEAR_COUNT_MAX);
    for (int i = 0; i < spear_n; ++i)
    {
        float angle = fwd_angle + randf(-SPEAR_CONE_HALF, SPEAR_CONE_HALF);
        Vector2 dir = { cosf(angle), sinf(angle) };
        if (!try_emit(effects, position, dir,
                      randf(SPEAR_SPEED_MIN, SPEAR_SPEED_MAX), splatter_color(), true))
            break;
    }

    // ── Fountain source (sustains spray for 0.3 s) ───────────────────
    FountainSource f;
    f.position   = position;
    f.direction  = fwd;
    f.age        = 0.0f;
    f.emit_timer = 0.0f;
    s_fountains.push_back(f);

    // ── Ooze sources (pool formation over 2-3 s) ─────────────────────
    int src_count = randi(OOZE_SRC_COUNT_MIN, OOZE_SRC_COUNT_MAX);
    for (int i = 0; i < src_count; ++i)
    {
        OozeSource src;
        float angle  = randf(0.0f, TWO_PI);
        float dist   = randf(0.0f, OOZE_SCATTER);
        src.position  = { position.x + cosf(angle) * dist,
                          position.y + sinf(angle) * dist };
        src.age           = 0.0f;
        src.duration      = randf(OOZE_DURATION_MIN, OOZE_DURATION_MAX);
        src.emit_timer    = 0.0f;
        src.emit_interval = randf(OOZE_INTERVAL_MIN, OOZE_INTERVAL_MAX);
        s_ooze_sources.push_back(src);
    }
}

void effects_spawn_player_death_blood(EffectsSystem *effects, Vector2 position,
                                      Vector2 bullet_direction)
{
    Vector2 fwd = (Vector2LengthSqr(bullet_direction) > 0.0001f)
        ? Vector2Normalize(bullet_direction)
        : Vector2{ 0.0f, 1.0f };
    float fwd_angle = atan2f(fwd.y, fwd.x);
    static constexpr float TWO_PI = 6.2831853f;

    // ── Heavy initial splatter in bullet direction ─────────────────────
    // Wide cone, fast, lots of particles — the entry wound spray.
    int burst_n = randi(250, 380);
    int fwd_n   = (int)(burst_n * 0.80f);
    for (int i = 0; i < burst_n; ++i)
    {
        float angle = (i < fwd_n)
            ? fwd_angle + randf(-1.2f, 1.2f)    // ±70° forward cone
            : randf(0.0f, TWO_PI);               // 20% fully random scatter
        Vector2 dir = { cosf(angle), sinf(angle) };
        if (!try_emit(effects, position, dir,
                      randf(400.0f, 1100.0f), splatter_color())) break;
    }

    // ── Spear pixels — fly across the screen, smear on walls ──────────
    for (int i = 0; i < randi(20, 35); ++i)
    {
        float angle = fwd_angle + randf(-0.9f, 0.9f);
        Vector2 dir = { cosf(angle), sinf(angle) };
        try_emit(effects, position, dir, randf(900.0f, 1400.0f), splatter_color(), true);
    }

    // ── Pump jets — upward arcs from heart pressure, 3-5 pulses ───────
    // Biased upward (negative Y) with randomised lateral drift.
    // Each jet originates from a slightly different point near the wound.
    int pump_count = randi(3, 5);
    for (int p = 0; p < pump_count; ++p)
    {
        // Random upward angle: between 200° and 340° (left-to-right overhead arc),
        // varied per pulse to break symmetry.
        float pump_angle = randf(3.49f, 5.93f); // 200°–340° in radians
        Vector2 pump_dir = { cosf(pump_angle), sinf(pump_angle) };
        Vector2 origin   = {
            position.x + randf(-8.0f, 8.0f),
            position.y + randf(-6.0f, 6.0f)
        };
        float pump_speed = randf(500.0f, 850.0f);
        int   jet_n      = randi(18, 30);
        for (int i = 0; i < jet_n; ++i)
        {
            float jitter = randf(-0.35f, 0.35f);
            float ang    = pump_angle + jitter;
            Vector2 dir  = { cosf(ang), sinf(ang) };
            try_emit(effects, origin, dir,
                     pump_speed * randf(0.6f, 1.0f), splatter_color());
        }
    }

    // ── Three fountain sources (longer duration than normal) ───────────
    // Stagger their positions to simulate pumping from the wound, not a point.
    for (int i = 0; i < 3; ++i)
    {
        FountainSource f;
        f.position   = {
            position.x + randf(-5.0f, 5.0f),
            position.y + randf(-5.0f, 5.0f)
        };
        // Each fountain sprays in a slightly different direction for chaos.
        float fan = fwd_angle + randf(-0.6f, 0.6f);
        f.direction  = { cosf(fan), sinf(fan) };
        f.age        = 0.0f;
        f.emit_timer = 0.0f;
        s_fountains.push_back(f);
    }

    // ── Dense ooze sources to form a large blood pool ─────────────────
    int src_n = randi(30, 45);
    for (int i = 0; i < src_n; ++i)
    {
        OozeSource src;
        float angle  = randf(0.0f, TWO_PI);
        float dist   = randf(0.0f, OOZE_SCATTER * 1.5f);
        src.position      = { position.x + cosf(angle) * dist,
                              position.y + sinf(angle) * dist };
        src.age           = 0.0f;
        src.duration      = randf(3.0f, 5.0f);   // longer than normal death
        src.emit_timer    = 0.0f;
        src.emit_interval = randf(OOZE_INTERVAL_MIN, OOZE_INTERVAL_MAX);
        s_ooze_sources.push_back(src);
    }
}

void effects_update(EffectsSystem *effects, const Tilemap *tm, float dt)
{
    static constexpr float TWO_PI = 6.2831853f;

    // ── Fountain emission ─────────────────────────────────────────────
    for (auto& f : s_fountains)
    {
        f.age        += dt;
        f.emit_timer += dt;

        if (f.emit_timer >= FOUNTAIN_INTERVAL)
        {
            f.emit_timer -= FOUNTAIN_INTERVAL;

            // Velocity tapers from full → 0 over the fountain lifetime.
            float taper   = 1.0f - (f.age / FOUNTAIN_DURATION);
            float fwd_angle = atan2f(f.direction.y, f.direction.x);
            int n = randi(FOUNTAIN_EMIT_MIN, FOUNTAIN_EMIT_MAX);
            for (int i = 0; i < n; ++i)
            {
                float angle = fwd_angle + randf(-FOUNTAIN_CONE_HALF, FOUNTAIN_CONE_HALF);
                Vector2 dir = { cosf(angle), sinf(angle) };
                float speed = randf(FOUNTAIN_SPEED_MIN, FOUNTAIN_SPEED_MAX) * taper;
                if (!try_emit(effects, f.position, dir, speed, splatter_color())) break;
            }
        }
    }

    s_fountains.erase(
        std::remove_if(s_fountains.begin(), s_fountains.end(),
                       [](const FountainSource& f) { return f.age >= FOUNTAIN_DURATION; }),
        s_fountains.end());

    // ── Ooze emission ─────────────────────────────────────────────────
    for (auto& src : s_ooze_sources)
    {
        src.age        += dt;
        src.emit_timer += dt;

        if (src.emit_timer >= src.emit_interval)
        {
            src.emit_timer -= src.emit_interval;
            int n = randi(OOZE_EMIT_MIN, OOZE_EMIT_MAX);
            for (int i = 0; i < n; ++i)
            {
                float angle = randf(0.0f, TWO_PI);
                Vector2 dir = { cosf(angle), sinf(angle) };
                // Ooze pixels use the settle palette — they're pooling blood, not fresh spray.
                if (!try_emit(effects, src.position, dir,
                              randf(OOZE_SPEED_MIN, OOZE_SPEED_MAX), settle_color())) break;
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

        // Wall collision: check the pixel centre in the new position.
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

        if (wall_hit && p.is_spear)
            spawn_wall_smear(effects, p.position);

        float speed_sq = Vector2LengthSqr(p.velocity);
        if (speed_sq < SETTLE_SPEED * SETTLE_SPEED || wall_hit || p.age >= p.max_lifetime)
            settle_pixel(effects, p);
    }

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
    s_fountains.clear();
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
