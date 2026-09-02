#include "common.h"

#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/task.h"

extern s8       D_8007106B;
extern TaskDesc RoomsShared8018397cDesc;

void RoomsShared8018397c(Task* arg0)
{
    Display_SpawnWithOt(&RoomsShared8018397cDesc, 1, arg0->spawnArg1, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    Task_Kill(arg0);
}
