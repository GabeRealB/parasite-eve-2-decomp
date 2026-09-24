#include "common.h"

#include "actors/actor_101500.h"
#include "main/task.h"

/// The actor's three task states - spawn, per-frame tick and teardown.
extern GpEnemyTaskFuncTable3 Actor01500_D00004;

/// Runs the task's current state handler from `Actor01500_D00004`, copying
/// the table onto the stack before the call.
void Actor01500_Fn02428(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor01500_D00004;
    sp.funcs[task->state](task->spawnArg2, task);
}
