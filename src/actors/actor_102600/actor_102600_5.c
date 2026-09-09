#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

extern GpEnemyTaskFuncTable3 D_actor_102600_80131E58;

void func_actor_102600_80135DA0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_102600_80131E58;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
