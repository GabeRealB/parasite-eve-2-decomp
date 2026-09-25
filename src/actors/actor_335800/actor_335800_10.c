#include "common.h"

#include "main/task.h"

/// Spawn, tick and teardown handlers of the child part tasks the parent block
/// spawns.
extern TaskFuncTable3 D_actor_335800_80161E24;

/// State dispatcher of the child part tasks: copies the three-handler table
/// onto the stack and runs the entry `Task::state` selects.
void func_actor_335800_80162E34(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_335800_80161E24;
    sp.funcs[task->state](task);
}
