#include "common.h"

#include "actors/actor_461800.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// `Task::exitCallback` of the first variant: hands the task's `GpEnemy`
/// (parked in `Task::spawnArg2` by the spawn descriptor) back to
/// `Gp_DestroyEnemy`, then kills the two helper tasks the spawn routine
/// started.
void func_actor_461800_80132A90(Task* task)
{
    Actor461800Work* work = (Actor461800Work*)task->work;

    Gp_DestroyEnemy(task->spawnArg2, task);
    taskKill(work->field_4F0);
    taskKill(work->field_4F4);
}
