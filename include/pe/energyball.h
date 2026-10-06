#ifndef INCLUDE_PE_ENERGYBALL_H
#define INCLUDE_PE_ENERGYBALL_H

#include "main/task_types.h"

void func_energyball_8012F180(Task* arg0);

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

void func_energyball_8012EF48(Task* arg0);

#endif // INCLUDE_PE_ENERGYBALL_H
