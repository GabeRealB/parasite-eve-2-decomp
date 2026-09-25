#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The actor's three state handlers: spawn, per-frame tick and teardown.
extern GpEnemyTaskFuncTable3 D_actor_312200_80161E24;

/// Runs the handler `Task::state` selects from `D_actor_312200_80161E24`,
/// passing the spawn argument and the task. The table is copied onto the stack
/// before the call.
void func_actor_312200_80163854(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_312200_80161E24;
    sp.funcs[task->state](task->spawnArg2, task);
}
