#include "weapon_manager.h"
#include "assault_rifle.h"
#include "deagle.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

// ── Constants ─────────────────────────────────────────────────────────────────
static constexpr float ORBIT_DIST       = 22.0f;
static constexpr float PICKUP_RANGE     = 40.0f;
static constexpr float THROW_SPEED      = 600.0f;
static constexpr float THROW_DRAG_BASE  = 0.88f;
static constexpr float THROW_STOP_SPEED = 10.0f;
static constexpr float BOB_SPEED        = 2.5f;
static constexpr float BOB_AMPLITUDE    = 2.0f;
static constexpr float ROT_LERP_SPEED   = 18.0f;
// RECOIL_DIST removed — each weapon reports its own recoil_distance().
static constexpr float RECOIL_DECAY     = 12.0f;
static constexpr float SIDE_OFFSET      = 6.0f;
static constexpr float RELOAD_BAR_W     = 32.0f;
static constexpr float RELOAD_BAR_H     = 4.0f;
static constexpr float RELOAD_BAR_Y_OFF = -50.0f;
static constexpr int   GAMEPAD_ID       = 0;

// ── Helpers ───────────────────────────────────────────────────────────────────

// Lerp angle in degrees, correctly wrapping through ±180°.
static float lerp_angle(float cur, float target, float speed, float dt)
{
    float diff = target - cur;
    while (diff >  180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    return cur + diff * std::min(speed * dt, 1.0f);
}

// World-space barrel tip: grip sits at ORBIT_DIST, barrel tip is one sprite-width further.
static Vector2 calc_muzzle(const Weapon& w, Vector2 player_ctr, Vector2 aim_dir)
{
    float dist = ORBIT_DIST + (float)w.sprite_width() * w.render_scale();
    return Vector2Add(player_ctr, Vector2Scale(aim_dir, dist));
}

// Tile-space solid check for a world-space point.
static bool point_solid(const Tilemap *tm, float wx, float wy)
{
    if (!tm) return false;
    int tx = (int)floorf(wx / (float)TILE_SIZE);
    int ty = (int)floorf(wy / (float)TILE_SIZE);
    return tilemap_is_solid(tm, tx, ty);
}

// ── Draw helpers ──────────────────────────────────────────────────────────────

// Draw a ground/thrown weapon centered at (cx, cy + bob_y).
// Uses the outline texture when on the ground (if the subclass provides one).
static void draw_ground_sprite(const Weapon& w, float cx, float cy,
                                float bob_y, bool use_outline)
{
    float sw = (float)w.sprite_width()  * w.render_scale();
    float sh = (float)w.sprite_height() * w.render_scale();
    float x  = cx - sw * 0.5f;
    float y  = cy - sh * 0.5f + bob_y;

    bool has_outline = use_outline && (w.outline_texture().id != 0);
    const Texture2D& tex = has_outline ? w.outline_texture() : w.texture();

    Rectangle src = { 0, 0, (float)tex.width, (float)tex.height };
    Rectangle dst = { x, y, sw, sh };
    Vector2   org = { 0, 0 };

    DrawTexturePro(tex, src, dst, org, 0.0f, WHITE);
}

// Draw the held weapon. Pivot at the grip (left/rear end of sprite).
// Flips vertically when aiming into the left hemisphere so the gun stays right-side up.
static void draw_held_sprite(const Weapon& w, Vector2 render_pos, float render_rotation)
{
    float sw = (float)w.sprite_width()  * w.render_scale();
    float sh = (float)w.sprite_height() * w.render_scale();

    float norm_rot = fmodf(render_rotation, 360.0f);
    if (norm_rot < 0.0f) norm_rot += 360.0f;
    bool flip = (norm_rot >= 90.0f && norm_rot <= 270.0f);

    Rectangle src = { 0, 0, (float)w.sprite_width(),
                      flip ? -(float)w.sprite_height() : (float)w.sprite_height() };
    Rectangle dst = { render_pos.x, render_pos.y - sh * 0.5f, sw, sh };
    Vector2   org = { 0.0f, sh * 0.5f };

    DrawTexturePro(w.texture(), src, dst, org, render_rotation, WHITE);
}

// ── Factory ───────────────────────────────────────────────────────────────────
// Map type_name string → concrete Weapon subclass.
// To add a new weapon: create the subclass, add an else-if here.

static std::unique_ptr<Weapon> create_weapon(std::string_view type_name)
{
    if (type_name == "assault_riffle")
        return std::make_unique<AssaultRifle>();
    if (type_name == "deagle")
        return std::make_unique<Deagle>();
    return nullptr;
}

// ── Public API ────────────────────────────────────────────────────────────────

void weapons_init(WeaponManager * /*wm*/)
{
    // Weapons are spawned on level load via weapons_spawn / weapons_load_from_tilemap.
    // Nothing to pre-load at the manager level — each Weapon subclass owns its assets.
}

int weapons_spawn(WeaponManager *wm, std::string_view type_name, Vector2 position)
{
    auto weapon = create_weapon(type_name);
    if (!weapon)
        return -1;

    weapon->position     = position;
    weapon->current_ammo = weapon->magazine_size();
    weapon->reserve_ammo = weapon->initial_reserve();
    weapon->is_on_ground = true;

    wm->weapons.push_back(std::move(weapon));
    return (int)wm->weapons.size() - 1;
}

void weapons_load_from_tilemap(WeaponManager *wm, const Tilemap *tm)
{
    if (!tm) return;
    for (const auto& obj : tm->objects)
    {
        if (obj.name != "weapon" && obj.type != "weapon") continue;

        auto it = obj.properties.find("weapon_type");
        if (it == obj.properties.end())
        {
            TraceLog(LOG_WARNING, "WEAPON: object at (%.0f,%.0f) missing 'weapon_type' property",
                     obj.x, obj.y);
            continue;
        }

        int idx = weapons_spawn(wm, it->second, { obj.x, obj.y });
        if (idx < 0)
            TraceLog(LOG_WARNING, "WEAPON: unknown weapon_type '%s'", it->second.c_str());
    }
}

void weapons_clear(WeaponManager *wm)
{
    wm->weapons.clear();
    wm->trigger_was_pressed = false;
}

void weapons_update(WeaponManager *wm, BulletSystem *bullets, AudioState *audio,
                    const Tilemap *tm, Vector2 player_center, Vector2 aim_direction,
                    bool input_blocked, float dt)
{
    // ── Find held weapon (at most one) ────────────────────────────────
    Weapon *held = nullptr;
    for (auto& w : wm->weapons)
        if (w->is_held) { held = w.get(); break; }

    // ── Ground bobbing ────────────────────────────────────────────────
    for (auto& w : wm->weapons)
        if (w->is_on_ground || w->is_thrown)
            w->bob_timer += dt * BOB_SPEED;

    // ── Thrown weapon physics ─────────────────────────────────────────
    float throw_drag = (dt > 0.0f) ? powf(THROW_DRAG_BASE, dt * 60.0f) : 1.0f;
    for (auto& w : wm->weapons)
    {
        if (!w->is_thrown) continue;

        Vector2 new_pos = Vector2Add(w->position, Vector2Scale(w->throw_velocity, dt));
        w->throw_velocity = Vector2Scale(w->throw_velocity, throw_drag);

        if (!point_solid(tm, new_pos.x, new_pos.y))
            w->position = new_pos;

        if (point_solid(tm, new_pos.x, new_pos.y) ||
            Vector2Length(w->throw_velocity) < THROW_STOP_SPEED)
        {
            w->is_thrown     = false;
            w->is_on_ground  = true;
            w->throw_velocity = {};
        }
    }

    if (!input_blocked)
    {
        // ── Pickup (E / controller Y) ─────────────────────────────────
        bool pickup_pressed =
            IsKeyPressed(KEY_E) ||
            IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_RIGHT_FACE_UP);

        if (pickup_pressed)
        {
            float best_dsq = PICKUP_RANGE * PICKUP_RANGE;
            Weapon *nearest = nullptr;
            for (auto& w : wm->weapons)
            {
                if (!w->is_on_ground) continue;
                float dsq = Vector2DistanceSqr(w->position, player_center);
                if (dsq < best_dsq) { best_dsq = dsq; nearest = w.get(); }
            }

            if (nearest)
            {
                if (held)
                {
                    held->is_held      = false;
                    held->is_on_ground = true;
                    held->position     = player_center;
                    held->is_reloading = false;
                    held = nullptr;
                }
                nearest->is_held         = true;
                nearest->is_on_ground    = false;
                nearest->render_rotation = atan2f(aim_direction.y, aim_direction.x) * RAD2DEG;
                held = nearest;
            }
        }

        // ── Throw (G / right bumper) ──────────────────────────────────
        bool throw_pressed = held &&
            (IsKeyPressed(KEY_G) ||
             IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_RIGHT_TRIGGER_1));

        if (throw_pressed && held->is_throwable())
        {
            held->is_held        = false;
            held->is_on_ground   = false;
            held->is_thrown      = true;
            held->position       = held->render_pos;
            held->throw_velocity = Vector2Scale(aim_direction, THROW_SPEED);
            held->is_reloading   = false;
            held = nullptr;
        }
    }
    else
    {
        wm->trigger_was_pressed = false;
    }

    // ── Held weapon: orbit, fire, reload ─────────────────────────────
    // Re-find held after potential pickup/throw swap above.
    held = nullptr;
    for (auto& w : wm->weapons)
        if (w->is_held) { held = w.get(); break; }

    if (held)
    {
        // Smoothly lerp rotation toward aim direction.
        float target_rot = atan2f(aim_direction.y, aim_direction.x) * RAD2DEG;
        held->render_rotation = lerp_angle(held->render_rotation, target_rot, ROT_LERP_SPEED, dt);

        // Position: grip at ORBIT_DIST in aim direction, offset perpendicular for visual clarity.
        float r = held->render_rotation * DEG2RAD;
        Vector2 perp = { -aim_direction.y, aim_direction.x };  // 90° left of aim
        held->render_pos = {
            player_center.x + cosf(r) * ORBIT_DIST + perp.x * SIDE_OFFSET - aim_direction.x * held->recoil_offset,
            player_center.y + sinf(r) * ORBIT_DIST + perp.y * SIDE_OFFSET - aim_direction.y * held->recoil_offset
        };

        // Decay recoil.
        held->recoil_offset -= held->recoil_offset * RECOIL_DECAY * dt;
        if (held->recoil_offset < 0.05f) held->recoil_offset = 0.0f;

        // Fire cooldown countdown.
        if (held->fire_cooldown_timer > 0.0f)
            held->fire_cooldown_timer -= dt;

        // Reload countdown.
        if (held->is_reloading)
        {
            held->reload_timer -= dt;
            if (held->reload_timer <= 0.0f)
            {
                held->is_reloading = false;
                held->reload_timer = 0.0f;
                // Pull from reserve to top off the magazine.
                int needed   = held->magazine_size() - held->current_ammo;
                int transfer = std::min(needed, held->reserve_ammo);
                held->current_ammo += transfer;
                held->reserve_ammo -= transfer;
            }
        }

        if (!input_blocked)
        {
            // Fire input: left click or right trigger.
            float trigger_val  = GetGamepadAxisMovement(GAMEPAD_ID, GAMEPAD_AXIS_RIGHT_TRIGGER);
            bool  trigger_held = trigger_val > 0.5f;
            bool  trigger_just = trigger_held && !wm->trigger_was_pressed;
            wm->trigger_was_pressed = trigger_held;

            bool fire_held    = IsMouseButtonDown(MOUSE_BUTTON_LEFT)    || trigger_held;
            bool fire_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || trigger_just;
            bool want_fire    = held->is_auto_fire() ? fire_held : fire_pressed;

            if (want_fire && held->fire_cooldown_timer <= 0.0f && !held->is_reloading)
            {
                if (held->current_ammo > 0)
                {
                    Vector2 muzzle = calc_muzzle(*held, player_center, aim_direction);
                    held->fire(bullets, muzzle, aim_direction);

                    held->current_ammo--;
                    held->fire_cooldown_timer = 1.0f / held->fire_rate();
                    held->recoil_offset      += held->recoil_distance();

                    held->play_shot_sound(audio->master_volume, audio->sfx_volume);
                }
                else if (!held->is_reloading && held->reserve_ammo > 0)
                {
                    // Auto-reload on empty magazine (only if reserve available).
                    held->is_reloading = true;
                    held->reload_timer = held->reload_time();
                    held->play_reload_sound(audio->master_volume, audio->sfx_volume);
                }
            }

            // Manual reload: R / controller X.
            bool reload_pressed =
                IsKeyPressed(KEY_R) ||
                IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);

            if (reload_pressed && !held->is_reloading &&
                held->current_ammo < held->magazine_size() &&
                held->reserve_ammo > 0)
            {
                held->is_reloading = true;
                held->reload_timer = held->reload_time();
                held->play_reload_sound(audio->master_volume, audio->sfx_volume);
            }
        }
    }
    else
    {
        wm->trigger_was_pressed = false;
    }
}

void weapons_draw_ground(const WeaponManager *wm, Vector2 player_center, bool controller_active)
{
    const char *prompt = controller_active ? "Y" : "E";

    for (const auto& w : wm->weapons)
    {
        if (w->is_held) continue;

        float bob = sinf(w->bob_timer) * BOB_AMPLITUDE;
        draw_ground_sprite(*w, w->position.x, w->position.y, bob, w->is_on_ground);

        float dsq = Vector2DistanceSqr(w->position, player_center);
        if (dsq < PICKUP_RANGE * PICKUP_RANGE)
        {
            float sh = (float)w->sprite_height() * w->render_scale();
            int   px = (int)(w->position.x - 4.0f);
            int   py = (int)(w->position.y - sh * 0.5f - 18.0f + bob);
            DrawText(prompt, px + 1, py + 1, 14, BLACK);
            DrawText(prompt, px,     py,     14, WHITE);
        }
    }
}

void weapons_draw_held(const WeaponManager *wm)
{
    for (const auto& w : wm->weapons)
    {
        if (!w->is_held) continue;
        draw_held_sprite(*w, w->render_pos, w->render_rotation);
    }
}

void weapons_draw_hud(const WeaponManager *wm, Vector2 player_center)
{
    for (const auto& w : wm->weapons)
    {
        if (!w->is_held) continue;

        if (w->is_reloading && w->reload_time() > 0.0f)
        {
            float fill = std::max(0.0f, 1.0f - w->reload_timer / w->reload_time());
            float bx   = player_center.x - RELOAD_BAR_W * 0.5f;
            float by   = player_center.y + RELOAD_BAR_Y_OFF;

            const int pad = 2;
            DrawRectangle((int)bx - pad, (int)by - pad,
                          (int)RELOAD_BAR_W + pad * 2, (int)RELOAD_BAR_H + pad * 2,
                          Color{ 20, 20, 20, 220 });
            DrawRectangle((int)bx, (int)by, (int)RELOAD_BAR_W, (int)RELOAD_BAR_H,
                          Color{ 60, 60, 60, 200 });
            DrawRectangle((int)bx, (int)by, (int)(RELOAD_BAR_W * fill), (int)RELOAD_BAR_H,
                          WHITE);
        }
        break; // at most one weapon held
    }
}

void weapons_draw_ammo_screen(const WeaponManager *wm, int screen_w, int screen_h)
{
    for (const auto& w : wm->weapons)
    {
        if (!w->is_held) continue;

        char ammo_buf[32];
        snprintf(ammo_buf, sizeof(ammo_buf), "%d / %d", w->current_ammo, w->reserve_ammo);

        const int font_size = 18;
        const int margin    = 16;
        int tw = MeasureText(ammo_buf, font_size);
        int tx = screen_w - tw - margin;
        int ty = screen_h - font_size - margin;

        DrawText(ammo_buf, tx + 1, ty + 1, font_size, BLACK);
        DrawText(ammo_buf, tx,     ty,     font_size, Color{ 220, 220, 220, 230 });
        break; // at most one weapon held
    }
}

void weapons_cleanup(WeaponManager *wm)
{
    wm->weapons.clear();  // unique_ptrs destroy weapons (assets unloaded in dtors)
    wm->trigger_was_pressed = false;
}

bool weapons_player_has_weapon(const WeaponManager *wm)
{
    for (const auto& w : wm->weapons)
        if (w->is_held) return true;
    return false;
}

void weapons_save_held(WeaponManager *wm)
{
    wm->held_save = {};
    for (const auto& w : wm->weapons)
    {
        if (!w->is_held) continue;
        wm->held_save.type_name    = std::string(w->type_name());
        wm->held_save.current_ammo = w->current_ammo;
        wm->held_save.reserve_ammo = w->reserve_ammo;
        wm->held_save.is_reloading = w->is_reloading;
        wm->held_save.reload_timer = w->reload_timer;
        wm->held_save.valid        = true;
        break;
    }
}

void weapons_restore_held(WeaponManager *wm)
{
    if (!wm->held_save.valid) return;

    int idx = weapons_spawn(wm, wm->held_save.type_name, {0.0f, 0.0f});
    if (idx >= 0)
    {
        Weapon& w       = *wm->weapons[idx];
        w.is_held       = true;
        w.is_on_ground  = false;
        w.current_ammo  = wm->held_save.current_ammo;
        w.reserve_ammo  = wm->held_save.reserve_ammo;
        w.is_reloading  = wm->held_save.is_reloading;
        w.reload_timer  = wm->held_save.reload_timer;
    }

    wm->held_save = {};
}
