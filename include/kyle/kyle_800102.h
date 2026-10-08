#ifndef INCLUDE_KYLE_KYLE_800102_H
#define INCLUDE_KYLE_KYLE_800102_H

#include "main/task_types.h"

/// Advances Kyle's MM1 grenade shell through launch, flight, blast and teardown.
///
/// Requires a live TMD task with state 0 spawn, 1 flight, 2 blast countdown or
/// 3 teardown; dispatch does not check bounds. At spawn the model root must be
/// parented to the firing weapon's root coordinate. `spawnArg1.value` packs
/// a `GRENADE_ROUND_*` ammunition index in bits 0..7, weapon index in 8..15,
/// launcher row in 16..19 (0 Grenade Pistol, 1 MM1), and companion-shot bit 20.
/// Kyle's MM1 uses row 1, weapon 12 and a companion fragmentation round.
///
/// Spawn allocates task-owned `WeaponGrenadeWork` and links the flight collision
/// bodies; teardown unlinks them before releasing work and the model. A handler
/// may release the task. Keep this package's code and model data loaded for the
/// projectile's lifetime.
void kyle800102GrenadeShellTask(Task* projectileTask);

#endif // INCLUDE_KYLE_KYLE_800102_H
