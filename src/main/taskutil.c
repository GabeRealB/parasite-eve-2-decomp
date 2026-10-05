#include "task.h"

#include "types.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/task.h"
#include "main/task_types.h"

extern u8 D_80725C54[];

void Task_KillMaybeSpawn(Task* task)
{
    if (gDisplayState.debugMode != 0) {
        taskSpawnFromTable((TaskDesc*)&D_80725C54, 0, 0, 0);
    }
    taskKill(task);
}
