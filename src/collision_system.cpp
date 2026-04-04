#include "collision_system.h"
#include "raymath.h"

void collision_bullets_vs_enemies(BulletSystem *bullets, EnemyManager *enemies,
                                  EffectsSystem *effects)
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
            effects_spawn_blood(effects, center, dir);
            return true;
        }
    }
    return false;
}
