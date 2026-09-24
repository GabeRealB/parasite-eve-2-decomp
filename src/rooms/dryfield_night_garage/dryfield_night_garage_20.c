#include "common.h"

#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/task.h"

extern s8 D_8007106B;

/// Descriptor of the task `func_dryfield_night_garage_80180D4C` spawns.
extern TaskDesc D_dryfield_night_garage_80183380;

/// Spawns `D_dryfield_night_garage_80183380` with an ordering table, passing
/// on the task's `spawnArg1`, sets `D_8007106B`, spawns the view tasks and
/// kills itself.
void func_dryfield_night_garage_80180D4C(Task* arg0)
{
    Display_SpawnWithOt(&D_dryfield_night_garage_80183380, 1, arg0->spawnArg1, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}
