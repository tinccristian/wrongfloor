#include "collision_system.h"
#include "raymath.h"
#include <cmath>
#include <cstdlib>

bool collision_melee_vs_player(const Weapon *weapon, Vector2 holder_pos, Vector2 aim_dir,
                               const Player *player, EffectsSystem *effects)
{
    if (!weapon || !player) return false;

    float len = Vector2Length(aim_dir);
    if (len < 0.0001f) return false;
    Vector2 fwd    = Vector2Scale(aim_dir, 1.0f / len);
    Vector2 side   = { -fwd.y, fwd.x };

    float range          = weapon->melee_range();
    float half_w         = weapon->melee_width() * 0.5f;
    float cone_half_angle = weapon->melee_cone_half_angle();
    bool  is_cone        = cone_half_angle > 0.0f;
    float cos_half       = is_cone ? cosf(cone_half_angle * DEG2RAD) : 0.0f;
    float origin_dist    = 22.0f + weapon->melee_origin_offset(); // ORBIT_DIST=22

    Vector2 origin  = Vector2Add(holder_pos, Vector2Scale(fwd, origin_dist));
    Vector2 p_center = player_center(player);
    Vector2 to_p    = Vector2Subtract(p_center, origin);
    float   dist    = Vector2Length(to_p);

    bool in_hitbox = false;
    if (is_cone)
    {
        if (dist <= range && dist > 0.0001f)
        {
            float cos_angle = Vector2DotProduct(fwd, Vector2Scale(to_p, 1.0f / dist));
            in_hitbox = (cos_angle >= cos_half);
        }
    }
    else
    {
        float along  = Vector2DotProduct(to_p, fwd);
        float across = Vector2DotProduct(to_p, side);
        in_hitbox = (along >= 0.0f && along <= range && fabsf(across) <= half_w);
    }

    if (in_hitbox && effects)
    {
        Vector2 dir = (dist > 0.01f) ? Vector2Scale(to_p, 1.0f / dist) : fwd;
        effects_spawn_player_death_blood(effects, p_center, dir);
    }

    return in_hitbox;
}

void collision_bullets_vs_enemies(BulletSystem *bullets, EnemyManager *enemies,
                                  EffectsSystem *effects, WeaponManager *wm)
{
    for (Bullet &bullet : bullets->bullets)
    {
        if (bullet.dead) continue;
        if (bullet.owner != BulletOwner::PLAYER) continue;

        for (Enemy &enemy : enemies->enemies)
        {
            if (!enemy.alive) continue;

            if (CheckCollisionPointRec(bullet.position, enemy_hitbox_rect(&enemy)))
            {
                enemy.alive    = false;
                enemy.ai_state = EnemyAIState::DEAD;
                bullet.dead    = true;

                Vector2 dir = (Vector2LengthSqr(bullet.velocity) > 0.0001f)
                    ? Vector2Normalize(bullet.velocity)
                    : Vector2{ 0.0f, 1.0f };
                effects_spawn_blood(effects, enemy.position, dir);

                // Drop the enemy's weapon as a pickable ground item with remaining ammo.
                if (enemy.weapon && wm)
                {
                    Weapon *w          = enemy.weapon.get();
                    w->position        = enemy.position;
                    w->is_held         = false;
                    w->is_on_ground    = true;
                    w->is_thrown       = false;
                    w->recoil_offset   = 0.0f;
                    w->render_rotation = enemy.weapon_render_rotation;
                    // current_ammo already reflects rounds fired; reserve stays full.
                    wm->weapons.push_back(std::move(enemy.weapon));
                }

                break;
            }
        }
    }
}

int collision_melee_vs_enemies(Vector2 origin, Vector2 aim_dir, float range, float width,
                               EnemyManager *enemies, EffectsSystem *effects,
                               WeaponManager *wm, float cone_half_angle)
{
    if (!enemies) return 0;

    float len = Vector2Length(aim_dir);
    if (len < 0.0001f) return 0;
    Vector2 fwd    = Vector2Scale(aim_dir, 1.0f / len);
    Vector2 side   = { -fwd.y, fwd.x };
    float   half_w = width * 0.5f;
    bool    is_cone = cone_half_angle > 0.0f;
    float   cos_half = is_cone ? cosf(cone_half_angle * DEG2RAD) : 0.0f;

    int hits = 0;
    for (Enemy &enemy : enemies->enemies)
    {
        if (!enemy.alive) continue;

        Vector2 to_e  = Vector2Subtract(enemy.position, origin);
        float   dist  = Vector2Length(to_e);

        bool in_hitbox = false;
        if (is_cone)
        {
            // Cone: distance within range AND angle within cone_half_angle of aim.
            if (dist <= range && dist > 0.0001f)
            {
                float cos_angle = Vector2DotProduct(fwd, Vector2Scale(to_e, 1.0f / dist));
                in_hitbox = (cos_angle >= cos_half);
            }
        }
        else
        {
            float along  = Vector2DotProduct(to_e, fwd);
            float across = Vector2DotProduct(to_e, side);
            in_hitbox = (along >= 0.0f && along <= range && fabsf(across) <= half_w);
        }

        if (in_hitbox)
        {
            enemy.alive    = false;
            enemy.ai_state = EnemyAIState::DEAD;

            Vector2 dir = (dist > 0.01f)
                ? Vector2Scale(to_e, 1.0f / dist)
                : fwd;
            effects_spawn_blood(effects, enemy.position, dir);

            // Drop enemy weapon as a ground item (same logic as bullet kills).
            if (enemy.weapon && wm)
            {
                Weapon *w       = enemy.weapon.get();
                w->position     = enemy.position;
                w->is_held      = false;
                w->is_on_ground = true;
                w->is_thrown    = false;
                w->recoil_offset = 0.0f;
                w->render_rotation = enemy.weapon_render_rotation;
                wm->weapons.push_back(std::move(enemy.weapon));
            }

            ++hits;
        }
    }
    return hits;
}

bool collision_bullets_vs_player(BulletSystem *bullets, const Player *player,
                                 EffectsSystem *effects)
{
    Rectangle hitbox = player_hitbox_rect(player);

    for (Bullet &bullet : bullets->bullets)
    {
        if (bullet.dead) continue;
        if (bullet.owner != BulletOwner::ENEMY) continue;

        if (CheckCollisionPointRec(bullet.position, hitbox))
        {
            bullet.dead = true;
            Vector2 center = player_center(player);
            Vector2 dir = (Vector2LengthSqr(bullet.velocity) > 0.0001f)
                ? Vector2Normalize(bullet.velocity)
                : Vector2{ 0.0f, 1.0f };

            effects_spawn_player_death_blood(effects, center, dir);
            return true;
        }
    }
    return false;
}
