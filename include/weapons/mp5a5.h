#ifndef INCLUDE_WEAPONS_MP5A5_H
#define INCLUDE_WEAPONS_MP5A5_H

#include "main/task_types.h"

/// Advances the MP5A5 alternate-fire muzzle flash and its transient point light.
///
/// Used by the base weapon and both upgrades. `task` must have a coordinate
/// body and own the `EffectWork` in `spawnArg2.pointer`, initialized by
/// `effectSpawn`. The work's borrowed parent coordinate and its ancestors must
/// remain live until teardown. The first visible update attaches the flash to
/// the weapon's muzzle; later updates halve its size and brightness.
/// Draws the textured core, additive screen tint and four streaks before
/// releasing the work and tearing down the task on the seventh visible update.
/// Hidden effects suspend updates, including aging and cleanup.
void mp5a5MuzzleFlashTask(Task* task);

/// Runs the MP5A5 player attack and repeats primary fire when its cooldown expires.
///
/// The two upgrade packages build this implementation with their weapon indices.
/// Primary fire clips at the first contact; secondary fire widens the shape,
/// attaches its flash to the weapon task and installs an 18-tick cooldown.
/// Tracks the lock target every dispatch.
///
/// Requires live player `GameActor` work, its model and initialized native
/// animation slots, equipped weapon/contact storage and the matching weapon
/// overlay loaded throughout dispatch and owned effects. Phase 0 enters
/// normal mode state 4; later calls advance `GameActor::statePhase`. Frame counts
/// are dispatch ticks. Releases its scratch reservation before returning.
/// Reserves 80 scratch bytes for a temporary impact node, in addition to
/// the called helpers' reservations.
void mp5a5AttackState(Task* playerTask);

#endif // INCLUDE_WEAPONS_MP5A5_H
