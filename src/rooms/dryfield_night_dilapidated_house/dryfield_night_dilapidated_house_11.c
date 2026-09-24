#include "common.h"

#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/task.h"

extern s8       D_8007106B;
extern TaskDesc D_dryfield_night_dilapidated_house_801872B4;

/// Entry 0 of the room's two-entry descriptor table: spawns entry 1, the
/// stream-playing task, with an ordering table, sets `D_8007106B`, spawns the
/// view tasks and kills itself.
void func_dryfield_night_dilapidated_house_8017DCE0(Task* arg0)
{
    Display_SpawnWithOt(&D_dryfield_night_dilapidated_house_801872B4, 1, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}
