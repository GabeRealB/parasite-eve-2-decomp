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

#endif // INCLUDE_WEAPONS_P229_H
