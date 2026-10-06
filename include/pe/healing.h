#ifndef INCLUDE_PE_HEALING_H
#define INCLUDE_PE_HEALING_H

#include "main/task_types.h"

/// Advances the additive rising spark emitted by a Healing sparkle.
///
/// Requires a counted effect task with a coordinate body and an owned, cleared
/// `EffectWork` in `spawnArg2.pointer`, as `Gp_SpawnEff` supplies. The parent
/// coordinate chain must remain live until teardown. `spawnArg1.value` bits
/// 0..11 give the signed-size drawer a nonnegative sizing numerator (0..4095).
/// The initialization tick chooses a fixed random screen rotation without
/// moving or drawing. Active ticks rise four parent-coordinate units along Y;
/// ages 3, 5, ... 15 draw texture frames 1..7. Age 16 releases the work and task,
/// unless parent-task teardown ends them earlier. Performs no room-control check.
void healingRisingSparkTask(Task* task);

void func_healing_8012F5E4(Task* arg0);

void func_healing_8012EF34(Task* arg0);

#endif // INCLUDE_PE_HEALING_H
