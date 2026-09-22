#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

extern GpEnemyTaskFuncTable3 Actor02600_D00038;

void Actor02600_Fn03F80(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02600_D00038;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
