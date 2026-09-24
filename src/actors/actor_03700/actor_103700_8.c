#include "common.h"

#include "actors/actor_103700.h"

extern GpEnemyTaskFuncTable3 Actor03700_D00004;

/// The actor's per-frame task callback: runs the handler for the task's state
/// from `Actor03700_D00004` (spawn, tick, death), passing the enemy record the
/// task was spawned with. The table is copied onto the stack before the call.
void Actor03700_Fn02FA8(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor03700_D00004;
    sp.funcs[task->state](task->spawnArg2, task);
}
