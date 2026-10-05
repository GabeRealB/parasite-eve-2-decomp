#include "task.h"

#include "types.h"

#include "main/display.h"
#include "main/task.h"

extern u8 D_80725C54[];

void taskDebugLaunchCallback(Task* task)
{
    if (gDisplayState.debugMode != 0) {
        // View the external debug address as a descriptor; its backing storage is unproven.
        taskSpawnFromTable((TaskDesc*)D_80725C54, 0, 0, 0);
    }
    taskKill(task);
}
