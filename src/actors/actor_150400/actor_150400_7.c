#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

#include "actors/actor_150400.h"

/// Exit callback of the actor's task: hands its `GpEnemy` back to
/// `Gp_DestroyEnemy`.
void func_actor_150400_801324B8(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}
