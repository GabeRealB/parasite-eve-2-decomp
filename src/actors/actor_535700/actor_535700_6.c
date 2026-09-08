#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

INCLUDE_ASM("actors/nonmatchings/actor_535700/actor_535700_6", ActorsShared80131e24Sub1);

void func_actor_535700_80132FF8(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_535700/actor_535700_6", func_actor_535700_80133020);
