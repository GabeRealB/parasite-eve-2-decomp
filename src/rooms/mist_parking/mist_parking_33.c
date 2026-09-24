#include "common.h"

#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/task.h"
#include "rooms/mist_parking.h"

extern s8 D_8007106B;

/// Spawns the display task `D_mist_parking_8018FC24` with the task's
/// `spawnArg1`, sets `D_8007106B`, respawns the view tasks and kills itself.
void func_mist_parking_8018397C(Task* arg0)
{
    Display_SpawnWithOt(&D_mist_parking_8018FC24, 1, arg0->spawnArg1, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}
