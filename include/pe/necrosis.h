#ifndef INCLUDE_PE_NECROSIS_H
#define INCLUDE_PE_NECROSIS_H

#include "main/task_types.h"

void func_necrosis_8012F52C(Task* arg0);

/// Advances and draws one drifting Necrosis mist puff from effect slot 0x01A.
///
/// Requires a coordinate body and the counted `EffectWork` created by the
/// effect spawner. The low 12 bits of `task->spawnArg1.value` supply its size
/// in game-coordinate units. Initialization chooses a fixed random bearing,
/// displacement and blend mode from the current PE level (1..3).
/// Draws frames 1..7 of the small strip, or 1..5 of the large strip at level 3
/// when subtractive blending is selected, then releases the work and task.
/// Updates, drawing and expiry stop while PE effect control is nonzero.
void necrosisMistPuffTask(Task* task);

void func_necrosis_8012EF34(Task* arg0);

#endif // INCLUDE_PE_NECROSIS_H
