#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's spawn, per-frame and teardown handlers, indexed by the task's
/// state.
extern GpEnemyTaskFuncTable3 Actor04000_D00240;

/// The enemy task's callback: runs the handler for the task's current state,
/// copying the table onto the stack before the call.
void Actor04000_Fn06E4C(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor04000_D00240;
    sp.funcs[task->state](task->spawnArg2, task);
}
