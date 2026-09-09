#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

extern GpEnemyTaskFuncTable3 D_actor_202600_80149E58;

void func_actor_202600_8014DDA0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_202600_80149E58;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
