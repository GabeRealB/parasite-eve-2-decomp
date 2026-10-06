#ifndef INCLUDE_WEAPONS_MP5A5_H
#define INCLUDE_WEAPONS_MP5A5_H

#include "main/task_types.h"

/// Advances the MP5A5 alternate-fire muzzle flash and its transient point light.
///
/// Used by the base weapon and both upgrades. `task` must have a coordinate
/// body and own the `EffectWork` in `spawnArg2.pointer`, initialized by
/// `Gp_SpawnEff`. The work's borrowed parent coordinate and its ancestors must
/// remain live until teardown. The first visible update attaches the flash to
/// the weapon's muzzle; later updates halve its size and brightness.
/// Draws the textured core, additive screen tint and four streaks before
/// releasing the work and tearing down the task on the seventh visible update.
/// Hidden effects suspend updates, including aging and cleanup.
void mp5a5MuzzleFlashTask(Task* task);

#endif // INCLUDE_WEAPONS_MP5A5_H
