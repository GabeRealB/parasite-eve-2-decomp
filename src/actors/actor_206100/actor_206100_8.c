#include "common.h"

#include "main/task.h"
#include "gameplay/1BC.h"
#include "actors/actor_206100.h"

void func_actor_206100_8014F490(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}
