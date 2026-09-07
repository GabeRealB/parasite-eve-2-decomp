#include "common.h"

#include "gameplay/1BC.h"

#include "main/task.h"

INCLUDE_ASM("actors/nonmatchings/actor_451100/actor_451100_4", ActorsShared80131e24Sub1);

void func_actor_451100_80132CAC(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_451100/actor_451100_4", func_actor_451100_80132CD4);
