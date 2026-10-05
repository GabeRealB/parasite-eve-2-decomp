#ifndef INCLUDE_PE_APOBIOSIS_H
#define INCLUDE_PE_APOBIOSIS_H

#include "main/task_types.h"

void func_apobiosis_8012EF4C(Task* arg0);

/// Updates and draws one short-lived apobiosis shard (bank-6 effect slot 0xF7).
///
/// Requires a spawned coordinate body and owned `EffectWork` in
/// `spawnArg2.pointer`, a live apobiosis cast, and a PE level digit in 1-3.
/// A nonzero `spawnArg1.value` pins the coordinate to the spawn parent for
/// 25 ticks; zero leaves it drifting for 17. Both modes draw a rotating sprite
/// and a strip on odd ticks. `EffectWork::step` holds the zero-based PE level
/// row, `index` the texture frame, `angle` the fixed screen rotation in 4096
/// units per turn, and `pos` the strip's world-space endpoint offset.
/// Reparents the task under the cast and frees its counted effect work when
/// its lifetime expires, effects are held, or room PE effects are cancelled.
void apobiosisShardTask(Task* task);

#endif // INCLUDE_PE_APOBIOSIS_H
