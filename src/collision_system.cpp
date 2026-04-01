#include "collision_system.h"
#include "raymath.h"

void collision_bullets_vs_enemies(BulletSystem *bullets, EnemyManager *enemies,
                                  EffectsSystem *effects)
{
    for (Bullet &bullet : bullets->bullets)
    {
        if (bullet.dead) continue;

        for (Enemy &enemy : enemies->enemies)
        {
            if (!enemy.alive) continue;

            float dist_sq = Vector2DistanceSqr(bullet.position, enemy.position);
            if (dist_sq < ENEMY_RADIUS * ENEMY_RADIUS)
            {
                enemy.alive = false;
                bullet.dead = true;

                Vector2 dir = (Vector2LengthSqr(bullet.velocity) > 0.0001f)
                    ? Vector2Normalize(bullet.velocity)
                    : Vector2{ 0.0f, 1.0f };
                effects_spawn_blood(effects, enemy.position, dir);
                break; // bullet consumed — move on to the next bullet
            }
        }
    }
}
