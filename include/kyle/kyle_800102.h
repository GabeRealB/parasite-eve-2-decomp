#ifndef INCLUDE_KYLE_KYLE_800102_H
#define INCLUDE_KYLE_KYLE_800102_H

#include "gameplay/animation.h"

#include "main/task_types.h"

/// Kyle Madigan's native companion animation bank for resource variant 2 (MM1).
///
/// Gameplay selects this bank at index 3, for companion family 1 variant 2.
/// The 47 base entries are clip pointers or NULL. The following 32 words start
/// NULL and stay writable for extension-copy messages, which can also install
/// raw payload words. Playback may follow only entries containing clip pointers.
/// Keep this package and any borrowed extension clips loaded during playback.
extern AnimationBank gKyle800102CompanionAnimationBank;

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
