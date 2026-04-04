#pragma once

#include "bullets.h"
#include "enemy.h"
#include "effects.h"
#include "player.h"

// Resolve PLAYER-owned bullet vs enemy overlaps.
// Kills each hit enemy, marks bullet dead, spawns blood.
void collision_bullets_vs_enemies(BulletSystem *bullets, EnemyManager *enemies,
                                  EffectsSystem *effects);

// Resolve ENEMY-owned bullet vs player overlap.
// Returns true if the player was hit (caller handles death).
bool collision_bullets_vs_player(BulletSystem *bullets, const Player *player,
                                 EffectsSystem *effects);
