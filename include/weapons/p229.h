#ifndef INCLUDE_WEAPONS_P229_H
#define INCLUDE_WEAPONS_P229_H

#include "main/task_types.h"

/// Advances the P229 alternate-fire muzzle flash and its transient white point light.
///
/// Spawned as `EFFECT_P229_MUZZLE_FLASH`. `task` must have a coordinate body
/// and own the `EffectWork` in `spawnArg2.pointer`, initialized by `effectSpawn`.
/// The work's borrowed parent coordinate and its ancestors must remain live
/// until teardown; the P229 overlay must remain loaded while the task runs.
/// The first visible update attaches the flash to the weapon's muzzle. Later
/// updates halve the core size and the tint and streak brightness.
/// Draws the textured core, additive screen tint and four streaks before
/// freeing the work and tearing down the task on the seventh visible update.
/// Hidden room effects suspend updates, including aging and cleanup.
void p229MuzzleFlashTask(Task* task);

/// Runs the P229 player attack with narrow primary or wide secondary contacts.
///
/// Primary fire consumes one primary load and reports a grid impact; secondary
/// fire consumes the secondary load, widens the weapon shape and attaches its
/// flash to the weapon task. Recovery installs a 12- or 18-tick cooldown.
///
/// Requires live player `GameActor` work, its model and initialized native
/// animation slots, equipped weapon/contact storage and the matching weapon
/// overlay loaded throughout dispatch and owned effects. Phase 0 enters
/// normal mode state 4; later calls advance `GameActor::statePhase`. Frame counts
/// are dispatch ticks. Releases its scratch reservation before returning.
/// Reserves 80 scratch bytes for a temporary impact node, in addition to
/// the called helpers' reservations.
void p229AttackState(Task* playerTask);

#endif // INCLUDE_WEAPONS_P229_H
