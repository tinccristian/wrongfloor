#pragma once

#include "bullets.h"
#include "enemy.h"
#include "effects.h"

// Resolve bullet-vs-enemy overlaps for the current frame.
// Kills each hit enemy (alive = false), marks the bullet dead, and spawns blood.
// Call after bullets_update and before effects_update.
void collision_bullets_vs_enemies(BulletSystem *bullets, EnemyManager *enemies,
                                  EffectsSystem *effects);

// FUTURE: add collision_player_vs_enemies(), collision_enemies_vs_walls(), etc.
