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

/// Accumulated tank yaw in 1/256 of a 4096th-turn angle, without angle wrapping.
///
/// The sway task adds the damped velocity once per running tick. Each room
/// defines its own initialized word, retained across task initialization for
/// the loaded overlay's lifetime. The day room shares it between its source files.
extern s32 gWaterTankYaw;

/// Tank angular velocity in yaw Q8 units per running tick.
///
/// Each tick stores (velocity + acceleration) * 19 / 20 with signed truncation
/// toward zero, then adds it to yaw. Each room defines its own initialized word;
/// its source files share that word for the loaded overlay's lifetime.
extern s32 gWaterTankYawSpeed;

/// Additive yaw acceleration before damping, in yaw Q8 units per tick squared.
///
/// Approaches target acceleration in increments of 256, including overshoot;
/// it remains a multiple of 256. Each room defines its own initialized word,
/// shared by its source files and retained for the loaded overlay's lifetime.
extern s32 gWaterTankYawAcceleration;

/// Random target for yaw acceleration, in yaw Q8 units per tick squared.
///
/// Reselected on the zero bucket of rand()*100/32768. A second bucket below 80
/// selects rand()*20/128 in 0..5119; otherwise the target is zero. Each room's
/// initialized word is shared by its source files and retained across sway task
/// starts within that loaded overlay. The target does not select a yaw position.
extern s32 gWaterTankYawTargetAcceleration;

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
