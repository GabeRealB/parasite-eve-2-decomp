#ifndef INCLUDE_PE_ENERGYBALL_H
#define INCLUDE_PE_ENERGYBALL_H

#include "main/task_types.h"

/// Charges, steers and retires one collision-bearing Energy Ball.
///
/// Bank-6 slot 0xF8 requires cleared owned `EffectWork` in `spawnArg2.pointer`
/// and a coordinate body. `spawnArg1.value` is the ball ordinal 0..2, selecting
/// transient light slot 4..6. Initialization selects PE level 1..3 from the
/// current attachment and allocates task-owned collision storage. The work
/// keeps size in `angle`, brightness in `scale`, sprite rotation in `period`,
/// flight velocity in `move`, and its steering target velocity in `pos`.
///
/// Charging raises the ball as size grows to the level limit. Flight links a
/// damage sphere of half that size, aims at the player every eighth running
/// update and steers on odd ages. Enemy contact spawns three impact rings and
/// starts a burst growing past twice full size; leaving battle starts shrinkage
/// below one size step. The last retiring ball stops the shared loop sound.
/// Non-running PE control below cancellation redraws without advancing.
/// Cancellation releases immediately; a held attachment also releases during
/// running updates once the attachment leaves the Energy Ball family.
/// Collision bodies are unlinked before their task-owned storage is released.
void energyballProjectileTask(Task* task);

/// Expands and fades one green ring from an energy ball impact.
///
/// Bank-6 effect F9 requires a coordinate body and an owned `EffectWork` in
/// `task->spawnArg2.pointer`. `spawnArg1.value` supplies a local Z rotation,
/// masked to 4096 units per turn. State 0 initializes; state 1 animates.
/// The work's `scale` holds green brightness and `angle` the inner radius in
/// game-coordinate units. The local-XZ band has width 384; radius grows by 128
/// and brightness falls by 8 after each draw, from initial values 256 and 128.
/// Draws 15 times, then releases the counted work and kills the task.
/// Nonzero PE effect control skips both drawing and updates, including cleanup.
void energyballImpactRingTask(Task* task);

/// Starts a level-dependent group of charging Energy Balls.
///
/// Bank-6 slot 0xCF requires a coordinate body, cleared owned `EffectWork` in
/// `spawnArg2.pointer`, and an Energy Ball attachment id (431..433). The level
/// digit selects one to three spawn offsets and the loop sound. Initializes
/// sixteen texture-phase jitters, then spawns balls at 1365-angle-unit intervals
/// (4096 units per turn), stopping when the existing flight count plus the
/// spawn ordinal reaches three. Negative flight counts are reset to zero.
/// The following callback releases this controller; it has no pause gate.
void energyballCastTask(Task* task);

#endif // INCLUDE_PE_ENERGYBALL_H
