/* Shared by two units of mist_parking. */

#include "engine/stage.h"
#include "engine/task.h"

static inline void _mistParkingDelayDisplayModeExit(Task* task)
{
    s32 ticksLeft;

    ticksLeft             = task->spawnArg1.value - 1;
    task->spawnArg1.value = ticksLeft;
    if (ticksLeft < 0) {
        taskKill(task);
        stageRequestModeTaskExit();
    }
}
