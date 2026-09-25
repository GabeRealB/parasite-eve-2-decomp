#include "common.h"

#include "actors/actor_143900.h"
#include "gameplay/1BC.h"

/// `Task::exitCallback` of the second variant: hands the task's `GpEnemy`
/// (parked in `Task::spawnArg2`) back to `Gp_DestroyEnemy`, then kills the two
/// helper tasks the spawn routine started.
void func_actor_143900_80132ECC(Task* task)
{
    Actor143900Work2* work = (Actor143900Work2*)task->work;

    Gp_DestroyEnemy(task->spawnArg2, task);
    taskKill(work->field_4F0);
    taskKill(work->field_4F4);
}
