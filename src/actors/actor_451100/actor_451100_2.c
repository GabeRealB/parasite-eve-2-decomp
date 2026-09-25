#include "common.h"

#include "actors/actor_451100.h"
#include "gameplay/1BC.h"
#include "main/task.h"

INCLUDE_ASM("actors/nonmatchings/actor_451100/actor_451100_2", func_actor_451100_80132330);

void func_actor_451100_801323B4(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}
