#include "common.h"

#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/task.h"

extern s8       D_8007106B;
extern TaskDesc RoomsShared8017daf0Desc;

void RoomsShared8017dce0(Task* arg0)
{
    Display_SpawnWithOt(&RoomsShared8017daf0Desc, 1, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    Task_Kill(arg0);
}
