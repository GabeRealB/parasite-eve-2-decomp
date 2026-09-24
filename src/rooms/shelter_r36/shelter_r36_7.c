#include "common.h"

#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/task.h"

extern s8       D_8007106B;
extern TaskDesc D_shelter_r36_8017E9A4[];

/// Entry 0 of `D_shelter_r36_8017E9A4`: spawns that table's entry 1, the
/// stream task `func_shelter_r36_8017DA34`, with an ordering table, passing on
/// this task's `spawnArg1`, sets `D_8007106B`, spawns the view tasks and ends.
void func_shelter_r36_8017DBC0(Task* arg0)
{
    Display_SpawnWithOt(D_shelter_r36_8017E9A4, 1, arg0->spawnArg1, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}
