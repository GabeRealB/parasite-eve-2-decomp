#ifndef INCLUDE_WEAPONS_TONFA_BATON_H
#define INCLUDE_WEAPONS_TONFA_BATON_H

#include "main/task_types.h"

void func_tonfa_baton_8011D1EC(Task* task);

/// Dispatches the attached tonfa baton model's lifecycle and pose update.
///
/// `task` must be live with a TMD body and a state in 0..3 (0 initialize,
/// 1 update pose, 2 defer removal, 3 teardown); spawning starts at state 0.
/// Pose updates require the player's live actor work and TMD body and the
/// baton's initialized root coordinate, parented to its attachment anchor.
/// The low nibble of `spawnArg1.value` selects 0 rest, 1 strike, otherwise
/// hold the angle; leaving the player's normal-mode attack resets it to rest.
/// Local angles use 4096 units per turn and the grip offset is 96 coordinate
/// units along the parent's Y axis. The tonfa baton overlay must remain loaded
/// through frame/exit dispatch; teardown follows `taskKill`'s lifetime rules.
void tonfaBatonModelTask(Task* task);

#endif // INCLUDE_WEAPONS_TONFA_BATON_H
