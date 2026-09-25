#include "common.h"

#include "actors/actor_350700.h"
#include "main/task.h"

/// State dispatcher of the child part tasks: copies the three-handler table
/// `D_actor_350700_80161E50` onto the stack and runs the entry `Task::state`
/// selects.
void func_actor_350700_80163274(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_350700_80161E50;
    sp.funcs[task->state](task);
}
