#include "common.h"

#include "main/task.h"

/// State table of the actor's child task (`TaskDesc` entry 1): setup, the
/// per-frame flag mirror and `taskKill`.
extern TaskFuncTable3 D_actor_443500_80131E24;

/// Dispatcher of the actor's child task: runs its current state handler from
/// `D_actor_443500_80131E24`, copying the table onto the stack before the call.
void func_actor_443500_8013253C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_443500_80131E24;
    sp.funcs[task->state](task);
}
