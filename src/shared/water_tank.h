/* The Dryfield water tank's model update, the same in the day and night builds:
 * the tank hangs against the room's north wall and sways about y.
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

void waterTankSwayTask(Task* arg0);

#endif /* SRC_SHARED_WATER_TANK_H */
