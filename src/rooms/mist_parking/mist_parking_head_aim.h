#ifndef SRC_ROOMS_MIST_PARKING_MIST_PARKING_HEAD_AIM_H
#define SRC_ROOMS_MIST_PARKING_MIST_PARKING_HEAD_AIM_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "main/task_types.h"

/// Head-aim update state and angular limits in 4096 units per turn.
enum {
    MIST_PARKING_HEAD_AIM_UPDATE    = 0,
    MIST_PARKING_HEAD_AIM_MAX_YAW   = ONE / 8,
    MIST_PARKING_HEAD_AIM_MAX_PITCH = ONE / 16
};

/// Advances the parking talk's head-aim weight by 1/16 toward its enabled state.
///
/// `killCountdown` starts at zero and holds a signed weight in 1/4096 units.
/// Animation or a nonzero `spawnArg1.value` enables aiming; normal updates
/// clamp the weight to 0..4096. The step wraps to 16 bits before the signed
/// clamp, retaining the counter's behavior even outside that normal range.
static inline void _mistParkingRampPlayerHeadAimBlend(Task* task, s32 animationRequestsAim)
{
    enum { MIST_PARKING_HEAD_AIM_BLEND_STEP = ONE / 16 };
    u16 blendWeight;

    if ((animationRequestsAim != 0) || (task->spawnArg1.value != 0)) {
        blendWeight         = task->killCountdown + MIST_PARKING_HEAD_AIM_BLEND_STEP;
        task->killCountdown = blendWeight;
        if ((s16)blendWeight >= ONE + 1) {
            task->killCountdown = ONE;
        }
    } else {
        blendWeight         = task->killCountdown - MIST_PARKING_HEAD_AIM_BLEND_STEP;
        task->killCountdown = blendWeight;
        if ((s16)blendWeight < 0) {
            task->killCountdown = 0;
        }
    }
}

#endif // SRC_ROOMS_MIST_PARKING_MIST_PARKING_HEAD_AIM_H
