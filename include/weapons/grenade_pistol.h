#ifndef INCLUDE_WEAPONS_GRENADE_PISTOL_H
#define INCLUDE_WEAPONS_GRENADE_PISTOL_H

#include "main/task_types.h"

/// Runs the player's Grenade Pistol ready, fire and recovery phases.
///
/// Requires live player `GameActor` work, native animation slots, equipment slot
/// 1 and the equipped weapon overlay. Phase 0 enters normal actor state 4;
/// phase 2 consumes one primary round and spawns a projectile using the current
/// ammunition index. The shot sets a 40-tick cooldown; recovery ends with the
/// selected character's weapon animation. `GameActor::statePhase` must be 0..3.
void grenadePistolAttackState(Task* playerTask);

/// Runs the same attack phases for the MM1's load, muzzle row and projectile.
///
/// Uses the Grenade Pistol attack contract with the MM1 overlay loaded.
void mm1AttackState(Task* playerTask);

void func_mm1_8011DBD0(Task* arg0);

/// Dispatches the Grenade Pistol projectile's spawn, flight, blast and teardown states.
///
/// Requires task state 0..3 and the matching weapon/gameplay overlays loaded.
/// Spawn owns the `WeaponGrenadeWork` allocation and initializes both collision
/// bodies; flight and blast require it live. Teardown unlinks both bodies and
/// releases the task/work. The projectile's packed spawn argument supplies
/// ammunition and launcher-row selectors to the states. The MM1 build exports
/// its own alias of this dispatcher; its model and launch tuning remain separate.
void grenadePistolShellTask(Task* task);

#endif // INCLUDE_WEAPONS_GRENADE_PISTOL_H
