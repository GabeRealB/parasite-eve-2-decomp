#include "common.h"

#include "main/task.h"
#include "rooms/neo_ark_shrine.h"

/// Task callback of the shrine's first falling prop: dispatches `Task::state`
/// through a copy of the prop's state table.
void func_neo_ark_shrine_8017EB54(Task* task)
{
    TaskFuncTable4 states;

    states = D_neo_ark_shrine_8017D610;
    states.funcs[task->state](task);
}
