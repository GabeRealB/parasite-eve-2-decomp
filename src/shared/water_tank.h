/* The Dryfield water tank's yaw sway, shared by the day and night rooms.
 * Each room supplies the model task and its persistent yaw-motion words.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_WATER_TANK_H
#define SRC_SHARED_WATER_TANK_H

#include "types.h"

#include "main/task_types.h"

/// The sway's accumulated yaw, in 1/256 of the matrix units.
extern s32 gWaterTankYaw;
/// Its velocity, eased 19/20 toward the step each frame.
extern s32 gWaterTankYawSpeed;
/// The step, moved 0x100 at a time toward the target.
extern s32 gWaterTankYawStep;
/// The target step, re-rolled now and then.
extern s32 gWaterTankYawTarget;

/// Updates the Dryfield tank's eased yaw sway and view-dependent visibility.
///
/// Requires a live TMD task with writable root and lighting matrices and the
/// room's persistent sway words. State 0 parents the root to the view and fixes
/// its room position; state 1 advances the sway. The yaw accumulator is in
/// 1/256 of a 4096th-turn angle. View 7 hides the model in either state.
/// Lighting samples the composed current position before replacing the yaw
/// basis and marking it dirty for later composition. Owns no additional storage.
void waterTankSwayTask(Task* task);

#endif /* SRC_SHARED_WATER_TANK_H */
