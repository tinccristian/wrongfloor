#include "weapon.h"
#include "game.h"      // assets_path()
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

// ── Constants ─────────────────────────────────────────────────────────────────
static constexpr float ORBIT_DIST       = 22.0f;   // px: player center → weapon center
static constexpr float PICKUP_RANGE     = 40.0f;   // px: max distance to pick up
static constexpr float THROW_SPEED      = 600.0f;  // px/sec initial throw speed
static constexpr float THROW_DRAG_BASE  = 0.88f;   // per-frame at 60 fps
static constexpr float THROW_STOP_SPEED = 10.0f;   // px/sec below which throw settles
static constexpr float BOB_SPEED        = 2.5f;    // rad/sec for ground bobbing
static constexpr float BOB_AMPLITUDE    = 2.0f;    // px of sine travel
static constexpr float ROT_LERP_SPEED   = 18.0f;   // rotation lerp speed — responsive but organic
static constexpr float RECOIL_DIST      = 4.0f;    // px kick per shot
static constexpr float RECOIL_DECAY     = 12.0f;   // lerp-back speed
static constexpr float SIDE_OFFSET      = 6.0f;    // px perpendicular to aim (offset from center line)
static constexpr float RELOAD_BAR_W     = 32.0f;   // px
static constexpr float RELOAD_BAR_H     = 4.0f;    // px
static constexpr float RELOAD_BAR_Y_OFF = -50.0f;  // above player center
static constexpr int   GAMEPAD_ID       = 0;

// ── Helpers ───────────────────────────────────────────────────────────────────

static float randf(float lo, float hi)
{
    return lo + (hi - lo) * ((float)std::rand() / (float)RAND_MAX);
}

// Lerp angle in degrees, correctly wrapping through ±180°.
static float lerp_angle(float cur, float target, float speed, float dt)
{
    float diff = target - cur;
    while (diff >  180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    float step = diff * std::min(speed * dt, 1.0f);
    return cur + step;
}

// World-space muzzle position — barrel tip of the gun.
// Grip (pivot) is at ORBIT_DIST from player center; barrel tip is at the far end of the sprite.
static Vector2 calc_muzzle(const WeaponData *data, Vector2 player_ctr, Vector2 aim_dir)
{
    float sprite_world_w = data->sprite_width * data->render_scale;
    float dist = ORBIT_DIST + sprite_world_w;  // grip → barrel tip = full sprite width
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

// ── Sprite drawing helpers ────────────────────────────────────────────────────

// Draw a ground/thrown weapon centered at (cx, cy + bob_y).
// When is_on_ground is true and an outline_texture is loaded, uses that sprite directly
// (pre-drawn outline baked into the PNG). Otherwise falls back to the plain texture.
static void draw_ground_sprite(const WeaponData *data, float cx, float cy,
                                float bob_y, bool use_outline)
{
    float sw = data->sprite_width  * data->render_scale;
    float sh = data->sprite_height * data->render_scale;
    float x  = cx - sw * 0.5f;
    float y  = cy - sh * 0.5f + bob_y;

    bool has_outline_tex = use_outline && (data->outline_texture.id != 0);
    Texture2D tex = has_outline_tex ? data->outline_texture : data->texture;

    // When using the outlined texture, expand src to its actual dimensions.
    Rectangle src = { 0, 0, (float)tex.width, (float)tex.height };
    Rectangle dst = { x, y, sw, sh };
    Vector2   org = { 0, 0 };

    DrawTexturePro(tex, src, dst, org, 0.0f, WHITE);
}

// Draw the held weapon with pivot at the grip (left/rear end of sprite).
// render_pos is the grip position in world space.
// When aiming into the left hemisphere (|angle| > 90°) the sprite is flipped vertically
// so the gun stays right-side up — no rotation adjustment needed beyond render_rotation itself.
static void draw_held_sprite(const WeaponData *data, Vector2 render_pos, float render_rotation)
{
    float sw = data->sprite_width  * data->render_scale;
    float sh = data->sprite_height * data->render_scale;

    // Normalize to [0, 360) and flip vertically when pointing into the left hemisphere.
    // This avoids edge cases with negative angles and is symmetric in both rotation directions.
    float norm_rot = fmodf(render_rotation, 360.0f);
    if (norm_rot < 0.0f) norm_rot += 360.0f;
    bool flip = (norm_rot >= 90.0f && norm_rot <= 270.0f);

    // src: negative height triggers the vertical flip in DrawTexturePro.
    Rectangle src = { 0, 0, (float)data->sprite_width,
                      flip ? -(float)data->sprite_height : (float)data->sprite_height };
    // dst: left edge at render_pos.x, vertically centered on render_pos.y.
    // org: pivot at the left-center of the dst rect (the grip end).
    Rectangle dst = { render_pos.x, render_pos.y - sh * 0.5f, sw, sh };
    Vector2   org = { 0.0f, sh * 0.5f };

    DrawTexturePro(data->texture, src, dst, org, render_rotation, WHITE);
}

// ── Public API ────────────────────────────────────────────────────────────────

void weapons_init(WeaponManager *wm)
{
    // ── Assault rifle ─────────────────────────────────────────────────
    WeaponData ar;
    ar.name          = "assault_riffle";
    ar.texture         = LoadTexture(assets_path("guns/AssaultRifles/assault_riffle.png").c_str());
    ar.outline_texture = LoadTexture(assets_path("guns/AssaultRifles/Outlined/M16.png").c_str());
    ar.sprite_width  = 35;
    ar.sprite_height = 11;
    ar.render_scale  = 2.0f;
    ar.fire_rate     = 10.0f;
    ar.magazine_size = 30;
    ar.reload_time   = 2.8f;
    ar.bullet_speed  = 900.0f;
    ar.bullet_damage = 1;
    ar.spread_angle  = 4.0f;
    ar.throwable     = true;
    ar.auto_fire     = true;
    ar.snd_shot   = LoadSound(assets_path("sounds/assault_riffle_burst.mp3").c_str());
    ar.snd_reload = LoadSound(assets_path("sounds/assault_riffle_reload.mp3").c_str());
    for (int i = 0; i < 4; ++i)
        ar.shot_aliases[i] = LoadSoundAlias(ar.snd_shot);
    wm->weapon_datas.push_back(std::move(ar));
    // Adding more guns: append more WeaponData entries here.
}

int weapons_spawn(WeaponManager *wm, const std::string& type_name, Vector2 position)
{
    for (const auto& wd : wm->weapon_datas)
    {
        if (wd.name == type_name)
        {
            Weapon w;
            // Pointer is stable: weapon_datas is only modified in weapons_init.
            w.data        = &wm->weapon_datas[&wd - wm->weapon_datas.data()];
            w.position    = position;
            w.current_ammo = wd.magazine_size;
            w.is_on_ground = true;
            wm->weapons.push_back(w);
            return (int)wm->weapons.size() - 1;
        }
    }
    return -1;
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
        if (w.is_held) { held = &w; break; }

    // ── Ground bobbing ────────────────────────────────────────────────
    for (auto& w : wm->weapons)
        if (w.is_on_ground || w.is_thrown)
            w.bob_timer += dt * BOB_SPEED;

    // ── Thrown weapon physics ─────────────────────────────────────────
    float throw_drag = (dt > 0.0f) ? powf(THROW_DRAG_BASE, dt * 60.0f) : 1.0f;
    for (auto& w : wm->weapons)
    {
        if (!w.is_thrown) continue;

        Vector2 new_pos = Vector2Add(w.position, Vector2Scale(w.throw_velocity, dt));
        w.throw_velocity = Vector2Scale(w.throw_velocity, throw_drag);

        bool wall = point_solid(tm, new_pos.x, new_pos.y);
        if (!wall) w.position = new_pos;

        if (wall || Vector2Length(w.throw_velocity) < THROW_STOP_SPEED)
        {
            w.is_thrown    = false;
            w.is_on_ground = true;
            w.throw_velocity = {};
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
                if (!w.is_on_ground) continue;
                float dsq = Vector2DistanceSqr(w.position, player_center);
                if (dsq < best_dsq) { best_dsq = dsq; nearest = &w; }
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
                nearest->is_held      = true;
                nearest->is_on_ground = false;
                nearest->render_rotation = atan2f(aim_direction.y, aim_direction.x) * RAD2DEG;
                held = nearest;
            }
        }

        // ── Throw (G / right bumper) ──────────────────────────────────
        bool throw_pressed = held &&
            (IsKeyPressed(KEY_G) ||
             IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_RIGHT_TRIGGER_1));

        if (throw_pressed)
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
    // Re-find held after potential swap above.
    held = nullptr;
    for (auto& w : wm->weapons)
        if (w.is_held) { held = &w; break; }

    if (held && held->data)
    {
        const WeaponData *data = held->data;

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
                held->current_ammo = data->magazine_size;
            }
        }

        if (!input_blocked)
        {
            // Fire input: left click (mouse) or right trigger (controller axis).
            float trigger_val    = GetGamepadAxisMovement(GAMEPAD_ID, GAMEPAD_AXIS_RIGHT_TRIGGER);
            bool  trigger_held   = trigger_val > 0.5f;
            bool  trigger_just   = trigger_held && !wm->trigger_was_pressed;
            wm->trigger_was_pressed = trigger_held;

            bool fire_held    = IsMouseButtonDown(MOUSE_BUTTON_LEFT) || trigger_held;
            bool fire_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || trigger_just;
            bool want_fire    = data->auto_fire ? fire_held : fire_pressed;

            if (want_fire && held->fire_cooldown_timer <= 0.0f && !held->is_reloading)
            {
                if (held->current_ammo > 0)
                {
                    // Spread: random angle offset within ±spread_angle degrees.
                    float spread = randf(-data->spread_angle, data->spread_angle) * DEG2RAD;
                    float base   = atan2f(aim_direction.y, aim_direction.x);
                    float ang    = base + spread;
                    Vector2 fire_dir = { cosf(ang), sinf(ang) };

                    Vector2 muzzle = calc_muzzle(data, player_center, aim_direction);
                    bullets_spawn(bullets, muzzle, fire_dir, data->bullet_speed);

                    held->current_ammo--;
                    held->fire_cooldown_timer = 1.0f / data->fire_rate;
                    held->recoil_offset      += RECOIL_DIST;

                    // Cycle through aliases so rapid fire can overlap.
                    Sound alias = data->shot_aliases[held->shot_alias_idx];
                    held->shot_alias_idx = (held->shot_alias_idx + 1) % 4;
                    SetSoundPitch(alias, randf(0.9f, 1.1f));
                    SetSoundVolume(alias, audio->master_volume * audio->sfx_volume);
                    PlaySound(alias);
                }
                else if (!held->is_reloading)
                {
                    // Auto-reload on empty magazine.
                    held->is_reloading = true;
                    held->reload_timer = data->reload_time;
                    SetSoundVolume(data->snd_reload, audio->master_volume * audio->sfx_volume);
                    PlaySound(data->snd_reload);
                }
            }

            // Manual reload: R key or controller X button.
            bool reload_pressed =
                IsKeyPressed(KEY_R) ||
                IsGamepadButtonPressed(GAMEPAD_ID, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);

            if (reload_pressed && !held->is_reloading &&
                held->current_ammo < data->magazine_size)
            {
                held->is_reloading = true;
                held->reload_timer = data->reload_time;
                SetSoundVolume(data->snd_reload, audio->master_volume * audio->sfx_volume);
                PlaySound(data->snd_reload);
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
        if (w.is_held || !w.data) continue;

        float bob = sinf(w.bob_timer) * BOB_AMPLITUDE;
        draw_ground_sprite(w.data, w.position.x, w.position.y, bob, w.is_on_ground);

        // Pickup prompt when player is close.
        float dsq = Vector2DistanceSqr(w.position, player_center);
        if (dsq < PICKUP_RANGE * PICKUP_RANGE)
        {
            float sh    = w.data->sprite_height * w.data->render_scale;
            int   px    = (int)(w.position.x - 4.0f);
            int   py    = (int)(w.position.y - sh * 0.5f - 18.0f + bob);
            DrawText(prompt, px + 1, py + 1, 14, BLACK);
            DrawText(prompt, px,     py,     14, WHITE);
        }
    }
}

// The held weapon always draws in front of the player.
void weapons_draw_held(const WeaponManager *wm)
{
    for (const auto& w : wm->weapons)
    {
        if (!w.is_held || !w.data) continue;
        draw_held_sprite(w.data, w.render_pos, w.render_rotation);
    }
}

// World-space: reload progress bar above the player. Call inside BeginMode2D.
void weapons_draw_hud(const WeaponManager *wm, Vector2 player_center)
{
    for (const auto& w : wm->weapons)
    {
        if (!w.is_held || !w.data) continue;

        if (w.is_reloading && w.data->reload_time > 0.0f)
        {
            float fill = std::max(0.0f, 1.0f - w.reload_timer / w.data->reload_time);
            float bx   = player_center.x - RELOAD_BAR_W * 0.5f;
            float by   = player_center.y + RELOAD_BAR_Y_OFF;

            // Dark border box (2px padding around the bar).
            const int pad = 2;
            DrawRectangle((int)bx - pad, (int)by - pad,
                          (int)RELOAD_BAR_W + pad * 2, (int)RELOAD_BAR_H + pad * 2,
                          Color{ 20, 20, 20, 220 });
            // Dark trough (unfilled portion).
            DrawRectangle((int)bx, (int)by, (int)RELOAD_BAR_W, (int)RELOAD_BAR_H,
                          Color{ 60, 60, 60, 200 });
            // Filled progress.
            DrawRectangle((int)bx, (int)by, (int)(RELOAD_BAR_W * fill), (int)RELOAD_BAR_H,
                          WHITE);
        }
        break; // at most one weapon held
    }
}

// Screen-space: ammo counter at the bottom-right corner. Call outside BeginMode2D.
void weapons_draw_ammo_screen(const WeaponManager *wm, int screen_w, int screen_h)
{
    for (const auto& w : wm->weapons)
    {
        if (!w.is_held || !w.data) continue;

        char ammo_buf[32];
        snprintf(ammo_buf, sizeof(ammo_buf), "%d / %d", w.current_ammo, w.data->magazine_size);

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
    for (auto& wd : wm->weapon_datas)
    {
        if (wd.texture.id != 0)         UnloadTexture(wd.texture);
        if (wd.outline_texture.id != 0) UnloadTexture(wd.outline_texture);
        // Unload aliases before the source sound.
        for (int i = 0; i < 4; ++i)
            if (wd.shot_aliases[i].stream.buffer) UnloadSoundAlias(wd.shot_aliases[i]);
        if (wd.snd_shot.stream.buffer)   UnloadSound(wd.snd_shot);
        if (wd.snd_reload.stream.buffer) UnloadSound(wd.snd_reload);
    }
    wm->weapon_datas.clear();
    wm->weapons.clear();
}

bool weapons_player_has_weapon(const WeaponManager *wm)
{
    for (const auto& w : wm->weapons)
        if (w.is_held) return true;
    return false;
}
