#include "common.h"

#include "main/task.h"

extern TaskFuncTable3 D_neo_ark_shrine_8017D620;

/// Task callback of the shrine's second falling prop: dispatches `Task::state`
/// through a copy of the prop's state table.
void func_neo_ark_shrine_8017EBB8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_shrine_8017D620;
    sp.funcs[task->state](task);
}
