#pragma once

#include "bullets.h"
#include "enemy.h"
#include "effects.h"
#include "player.h"
#include "weapon_manager.h"

// Resolve PLAYER-owned bullet vs enemy overlaps.
// Kills each hit enemy, marks bullet dead, spawns blood, and drops the enemy's
// weapon into wm as a pickable ground item with its remaining ammo.
void collision_bullets_vs_enemies(BulletSystem *bullets, EnemyManager *enemies,
                                  EffectsSystem *effects, WeaponManager *wm);

// Resolve ENEMY-owned bullet vs player overlap.
// Returns true if the player was hit (caller handles death).
// Spawns a large blood burst at the player center on hit.
bool collision_bullets_vs_player(BulletSystem *bullets, const Player *player,
                                 EffectsSystem *effects);
