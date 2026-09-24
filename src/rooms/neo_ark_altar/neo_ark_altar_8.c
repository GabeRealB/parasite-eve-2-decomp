#include "common.h"

#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/task.h"

extern s8       D_8007106B;
extern TaskDesc D_neo_ark_altar_8017EFC0[];

/// Entry 0 of `D_neo_ark_altar_8017EFC0`: spawns that table's entry 1 (the
/// streaming task `func_neo_ark_altar_8017DA40`) with an ordering table,
/// passing on this task's `spawnArg1`, sets `D_8007106B`, calls
/// `Gp_SpawnViewTasks` and ends.
void func_neo_ark_altar_8017DBF0(Task* arg0)
{
    Display_SpawnWithOt(D_neo_ark_altar_8017EFC0, 1, arg0->spawnArg1, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}
