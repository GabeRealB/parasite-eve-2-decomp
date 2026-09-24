#include "common.h"

#include "main/task.h"

#include "gameplay/1BC.h"

#include "actors/actor_215100.h"

/// Exit callback: hands the task's `GpEnemy` back to `Gp_DestroyEnemy`.
void func_actor_215100_8014CB04(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}
