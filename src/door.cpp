#include "door.h"
#include "player.h"
#include "enemy.h"
#include "weapon_manager.h"
#include "bullets.h"
#include "effects.h"
#include "raymath.h"
#include <cmath>
#include <algorithm>

// ── Constants ─────────────────────────────────────────────────────────────────
static constexpr float DRAG              = 0.88f;   // velocity multiplier per "tick" at 60 fps
static constexpr float SLAM_PLAYER       = 16.0f;   // rad/s applied when player contacts door
static constexpr float SLAM_ENEMY        = 14.0f;
static constexpr float SLAM_BULLET       = 8.0f;
static constexpr float SLAM_THROW        = 12.0f;
static constexpr float SLAM_MELEE        = 20.0f;
static constexpr float SLAM_KILL_THRESH  = 8.0f;    // rad/s needed to start the kill window
static constexpr float SLAM_KILL_WINDOW  = 0.5f;    // seconds the kill window stays open
static constexpr float MAX_SWING         = PI * 0.5f; // 90 degrees hard clamp each direction
static constexpr float REST_AV           = 0.5f;    // rad/s: door is "resting" below this
static constexpr float OPEN_ANGLE_THRESH = PI * 0.25f; // 45° from closed = considered "open"
static constexpr float PLAYER_MARGIN     = 3.0f;    // px expansion for player overlap test
static constexpr float ENEMY_MARGIN      = 4.0f;

// ── Helpers ───────────────────────────────────────────────────────────────────

// Transform world-space point into the door's local coordinate frame.
// lx: 0 = hinge, length = free end.
// ly: 0 = door centre line, positive = "left" side when looking from hinge to free end.
static void to_local(const Door& d, Vector2 point, float *lx_out, float *ly_out)
{
    float ca = cosf(d.angle), sa = sinf(d.angle);
    float rx = point.x - d.hinge_position.x;
    float ry = point.y - d.hinge_position.y;
    *lx_out =  rx * ca + ry * sa;
    *ly_out = -rx * sa + ry * ca;
}

// True if point is within the door rectangle, each side expanded by margin.
static bool point_in_door(const Door& d, Vector2 point, float margin = 0.0f)
{
    float lx, ly;
    to_local(d, point, &lx, &ly);
    return lx >= -margin && lx <= d.length + margin &&
           fabsf(ly) <= d.thickness * 0.5f + margin;
}

// World-space push delta to slide a point out through the nearest thickness face.
// Returns {0,0} if no penetration.
static Vector2 door_push_normal(const Door& d, Vector2 point)
{
    float lx, ly;
    to_local(d, point, &lx, &ly);
    float half_t = d.thickness * 0.5f;
    float pen = half_t - fabsf(ly) + 0.5f; // extra 0.5 px to fully clear
    if (pen <= 0.0f) return { 0.0f, 0.0f };
    float side = (ly >= 0.0f) ? 1.0f : -1.0f;
    // Door normal in world space = (-sin a, cos a) * side
    float sa = sinf(d.angle), ca = cosf(d.angle);
    return { -sa * side * pen, ca * side * pen };
}

// Apply angular velocity to swing the free end AWAY from the contact point.
// Only overrides if the new speed has greater magnitude (doesn't slow a fast swing).
// If the resulting speed exceeds SLAM_KILL_THRESH, opens the kill window.
static void door_apply_contact(Door& d, Vector2 contact_pos, float slam_speed)
{
    float ca = cosf(d.angle), sa = sinf(d.angle);
    float rx = contact_pos.x - d.hinge_position.x;
    float ry = contact_pos.y - d.hinge_position.y;
    // Cross product of door direction × to_contact gives handedness.
    float cross = ca * ry - sa * rx;
    if (fabsf(cross) < 0.01f) return; // contact on the door axis — no torque
    float new_av = copysignf(slam_speed, -cross); // negative cross → swing clockwise
    if (fabsf(new_av) > fabsf(d.angular_velocity))
        d.angular_velocity = new_av;
}

// Unit vector in the direction the free end is swinging (for blood splatter).
static Vector2 swing_direction(const Door& d)
{
    // Tangential velocity direction = d/dθ (cos θ, sin θ) = (-sin θ, cos θ).
    // Positive av → clockwise in screen space → tip moves in (-sin θ, cos θ) * sign(av)?
    // Verify at θ=0, av>0: tip at (length,0) moves toward (length·cos ε, length·sin ε) ≈ +Y.
    // (-sin 0, cos 0)*1 = (0,1) ✓
    float sa = sinf(d.angle), ca = cosf(d.angle);
    float sign = (d.angular_velocity >= 0.0f) ? 1.0f : -1.0f;
    return { -sa * sign, ca * sign };
}

// ── Public API ────────────────────────────────────────────────────────────────

void doors_load_from_tilemap(DoorSystem *ds, const Tilemap *tm)
{
    if (!tm) return;
    for (const auto& obj : tm->objects)
    {
        if (obj.name != "door" && obj.type != "door") continue;

        Door d;
        bool horizontal = (obj.width >= obj.height);
        d.length    = horizontal ? obj.width  : obj.height;
        d.thickness = horizontal ? obj.height : obj.width;

        // Default hinge at "start" (left edge for horizontal, top edge for vertical).
        std::string hinge_prop = "start";
        auto it = obj.properties.find("hinge");
        if (it != obj.properties.end()) hinge_prop = it->second;

        if (horizontal)
        {
            float cy = obj.y + obj.height * 0.5f;
            if (hinge_prop == "start")
            {
                d.hinge_position = { obj.x, cy };
                d.closed_angle   = 0.0f; // extends along +X
            }
            else // "end" → hinge at right edge, door extends left
            {
                d.hinge_position = { obj.x + obj.width, cy };
                d.closed_angle   = PI; // extends along -X
            }
        }
        else // vertical
        {
            float cx = obj.x + obj.width * 0.5f;
            if (hinge_prop == "start")
            {
                d.hinge_position = { cx, obj.y };
                d.closed_angle   = PI * 0.5f; // extends along +Y (downward)
            }
            else // "end" → hinge at bottom, door extends upward
            {
                d.hinge_position = { cx, obj.y + obj.height };
                d.closed_angle   = -PI * 0.5f; // extends along -Y
            }
        }

        d.angle            = d.closed_angle;
        d.angular_velocity = 0.0f;
        d.is_open          = false;
        ds->doors.push_back(d);
    }
}

void doors_clear(DoorSystem *ds)
{
    ds->doors.clear();
}

bool doors_update(DoorSystem *ds, Player *player, EnemyManager *enemies,
                  BulletSystem *bullets, WeaponManager *weapons,
                  EffectsSystem *effects, float dt)
{
    bool player_killed = false;

    // Drag factor: framerate-independent via powf.
    float drag = (dt > 0.0f) ? powf(DRAG, dt * 60.0f) : 1.0f;

    for (Door& d : ds->doors)
    {
        // ── Physics ───────────────────────────────────────────────────────
        d.angle += d.angular_velocity * dt;
        d.angular_velocity *= drag;
        if (fabsf(d.angular_velocity) < 0.05f) d.angular_velocity = 0.0f;

        // Hard clamp ±90° with a small bounce at the stops.
        float max_a = d.closed_angle + MAX_SWING;
        float min_a = d.closed_angle - MAX_SWING;
        if (d.angle > max_a)
        {
            d.angle = max_a;
            if (d.angular_velocity > 0.0f)
                d.angular_velocity = -d.angular_velocity * 0.55f;
        }
        else if (d.angle < min_a)
        {
            d.angle = min_a;
            if (d.angular_velocity < 0.0f)
                d.angular_velocity = -d.angular_velocity * 0.55f;
        }

        // Snap closed when nearly back to rest to prevent infinite micro-oscillation.
        if (fabsf(d.angle - d.closed_angle) < 0.05f && fabsf(d.angular_velocity) < REST_AV)
        {
            d.angle            = d.closed_angle;
            d.angular_velocity = 0.0f;
        }

        // Update open/closed state.
        d.is_open = (fabsf(d.angular_velocity) < REST_AV) &&
                    (fabsf(d.angle - d.closed_angle) > OPEN_ANGLE_THRESH);

        // Decrement slam kill window.
        if (d.slam_kill_timer > 0.0f) d.slam_kill_timer -= dt;

        // ── Slam kills (0.5s window after a hard slam → kills enemies) ───
        if (d.slam_kill_timer > 0.0f)
        {
            Vector2 sdir = swing_direction(d);

            if (enemies)
            {
                for (Enemy& e : enemies->enemies)
                {
                    if (!e.alive) continue;
                    // Use enemy hitbox half-width as overlap margin for generous hits.
                    if (!point_in_door(d, e.position, ENEMY_HITBOX_W * 0.5f)) continue;

                    e.alive    = false;
                    e.ai_state = EnemyAIState::DEAD;
                    if (effects) effects_spawn_blood(effects, e.position, sdir);

                    // Drop enemy weapon into the world.
                    if (e.weapon && weapons)
                    {
                        Weapon *ew          = e.weapon.get();
                        ew->position        = e.position;
                        ew->is_held         = false;
                        ew->is_on_ground    = true;
                        ew->is_thrown       = false;
                        ew->recoil_offset   = 0.0f;
                        ew->render_rotation = e.weapon_render_rotation;
                        weapons->weapons.push_back(std::move(e.weapon));
                    }
                }
            }

            // Player is NOT killed by slam — only enemies are. The player pushes doors.
        }

        // ── Player contact: push out + swing door ─────────────────────────
        if (player && !player_killed)
        {
            Vector2 pctr = player_center(player);
            if (point_in_door(d, pctr, PLAYER_MARGIN))
            {
                Vector2 push = door_push_normal(d, pctr);
                player->position.x += push.x;
                player->position.y += push.y;
                door_apply_contact(d, pctr, SLAM_PLAYER);
                if (fabsf(d.angular_velocity) >= SLAM_KILL_THRESH)
                    d.slam_kill_timer = SLAM_KILL_WINDOW;
            }
        }

        // ── Bullet contacts ───────────────────────────────────────────────
        if (bullets)
        {
            for (Bullet& b : bullets->bullets)
            {
                if (b.dead) continue;
                // Only player-owned bullets open the kill window.
                if (point_in_door(d, b.position))
                {
                    bool was_player = (b.owner == BulletOwner::PLAYER);
                    b.dead = true;
                    door_apply_contact(d, b.position, SLAM_BULLET);
                    if (was_player && fabsf(d.angular_velocity) >= SLAM_KILL_THRESH)
                        d.slam_kill_timer = SLAM_KILL_WINDOW;
                }
            }
        }

        // ── Thrown weapon contacts ────────────────────────────────────────
        if (weapons)
        {
            for (auto& w : weapons->weapons)
            {
                if (!w->is_thrown) continue;
                if (point_in_door(d, w->position, 4.0f))
                {
                    w->is_thrown        = false;
                    w->is_on_ground     = true;
                    w->throw_velocity   = {};
                    w->throw_spin_speed = 0.0f;
                    door_apply_contact(d, w->position, SLAM_THROW);
                    if (fabsf(d.angular_velocity) >= SLAM_KILL_THRESH)
                        d.slam_kill_timer = SLAM_KILL_WINDOW;
                }
            }
        }

        // ── Enemy contacts: push out + swing door ─────────────────────────
        if (enemies)
        {
            for (Enemy& e : enemies->enemies)
            {
                if (!e.alive) continue;
                if (point_in_door(d, e.position, ENEMY_MARGIN))
                {
                    Vector2 push = door_push_normal(d, e.position);
                    e.position.x += push.x;
                    e.position.y += push.y;
                    door_apply_contact(d, e.position, SLAM_ENEMY);
                }
            }
        }

        // ── Melee weapon contact: check arc points while swinging ─────────
        if (weapons && player)
        {
            for (const auto& w : weapons->weapons)
            {
                if (!w->is_held || !w->is_melee() || !w->is_swinging) continue;
                Vector2 aim         = player->aim.direction;
                Vector2 pctr        = player_center(player);
                float   origin_dist = 22.0f + w->melee_origin_offset(); // ORBIT_DIST=22
                float   range       = w->melee_range();
                // Check at origin and tip of the melee arc.
                Vector2 pts[2] = {
                    { pctr.x + aim.x * origin_dist,          pctr.y + aim.y * origin_dist },
                    { pctr.x + aim.x * (origin_dist + range), pctr.y + aim.y * (origin_dist + range) }
                };
                for (const auto& pt : pts)
                {
                    if (point_in_door(d, pt, 6.0f))
                    {
                        door_apply_contact(d, pt, SLAM_MELEE);
                        if (fabsf(d.angular_velocity) >= SLAM_KILL_THRESH)
                            d.slam_kill_timer = SLAM_KILL_WINDOW;
                        break;
                    }
                }
                break; // at most one held weapon
            }
        }
    }

    return player_killed;
}

void doors_draw(const DoorSystem *ds)
{
    static const Color DOOR_COLOR   = { 90, 65, 40, 255 };
    static const Color BORDER_COLOR = { 150, 115, 70, 255 };

    for (const Door& d : ds->doors)
    {
        // Filled body — origin at (0, thickness/2) so it pivots around the hinge.
        Rectangle rect   = { d.hinge_position.x, d.hinge_position.y,
                              d.length, d.thickness };
        Vector2   origin = { 0.0f, d.thickness * 0.5f };
        DrawRectanglePro(rect, origin, d.angle * RAD2DEG, DOOR_COLOR);

        // Lighter border: draw the three exposed edges (free end + two long sides).
        float ca = cosf(d.angle), sa = sinf(d.angle);
        float half_t = d.thickness * 0.5f;
        // Perpendicular (left normal) in world space
        Vector2 along = { ca,  sa };
        Vector2 perp  = { -sa, ca };

        // Four corners
        Vector2 ht = { d.hinge_position.x + perp.x * half_t,
                        d.hinge_position.y + perp.y * half_t };
        Vector2 hb = { d.hinge_position.x - perp.x * half_t,
                        d.hinge_position.y - perp.y * half_t };
        Vector2 ft = { ht.x + along.x * d.length, ht.y + along.y * d.length };
        Vector2 fb = { hb.x + along.x * d.length, hb.y + along.y * d.length };

        // Long sides and free end — skip the hinge edge (it's attached to the wall).
        DrawLineEx(ht, ft, 1.5f, BORDER_COLOR);
        DrawLineEx(hb, fb, 1.5f, BORDER_COLOR);
        DrawLineEx(ft, fb, 2.0f, BORDER_COLOR); // free end slightly thicker
    }
}
