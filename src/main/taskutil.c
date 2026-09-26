#include "common.h"

#include "main/display.h"
#include "main/stage.h"
#include "main/unknown_syms.h"

void Task_KillMaybeSpawn(Task* arg0)
{
    if (gDisplayState.field_112 != 0) {
        Task_SpawnFromTable((TaskDesc*)&D_80725C54, 0, 0, 0);
    }
    taskKill(arg0);
}
