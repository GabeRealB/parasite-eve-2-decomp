#ifndef INCLUDE_WEAPONS_M4A1_GRENADE_H
#define INCLUDE_WEAPONS_M4A1_GRENADE_H

#include "main/task_types.h"

/// Runs the M4A1 grenade launcher's rifle burst or single grenade attack.
///
/// Requires live player `GameActor` work, native animation slots, the six-entry
/// weapon contact table, equipment slot 1 and the M4A1 Grenade overlay. Primary
/// fire uses up to three rifle rounds, with three countdown ticks between shots;
/// secondary fire uses one grenade and launches its projectile. The secondary
/// load's ammunition index selects sound variants, defaulting to fragmentation
/// when empty. A live load must contain one of the three compatible 40mm rounds.
/// `GameActor::statePhase` must be 0..5. Reserves one temporary `GfxCoord` for room
/// impact sounds and releases it before returning; only its cached translation
/// is supplied by the impact query. Recovery permits cancellation after the
/// attack's delay, then sets a 12-tick cooldown.
void m4a1GrenadeAttackState(Task* playerTask);

/// Dispatches an M4A1 grenade's launch, flight, blast countdown and teardown.
///
/// Bank-7 descriptor 0x60 supplies the projectile model. `Task::state` must be
/// 0..3; the weapon and gameplay overlays must remain loaded. Launch allocates
/// `WeaponGrenadeWork` and links its sphere and capsule; later states require
/// that work until teardown unlinks both bodies and releases the task.
/// The projectile borrows the muzzle coordinate only during initialization,
/// then flies parented to the view coordinate. Detonation reads the player's
/// current secondary load rather than the ammunition packed at launch.
void m4a1GrenadeShellTask(Task* task);

#endif // INCLUDE_WEAPONS_M4A1_GRENADE_H
