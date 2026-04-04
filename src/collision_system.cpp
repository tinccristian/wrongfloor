#include "collision_system.h"
#include "raymath.h"
#include <cmath>

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
