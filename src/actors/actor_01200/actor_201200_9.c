#include "common.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// The actor's three task states: spawn, per-frame tick and teardown.
extern GpEnemyTaskFuncTable3 Actor01200_D0010C;

/// Task entry point: runs the handler for the task's current state from a
/// stack copy of `Actor01200_D0010C`.
void Actor01200_Fn03FD4(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor01200_D0010C;
    sp.funcs[task->state](task->spawnArg2, task);
}
