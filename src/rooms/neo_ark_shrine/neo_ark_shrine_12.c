#include "common.h"

#include "main/task.h"
#include "rooms/neo_ark_shrine.h"

/// Task callback of the shrine's cap script: dispatches `Task::state` through a
/// copy of the script's state table.
void func_neo_ark_shrine_8017EAE0(Task* task)
{
    TaskFuncTable16 sp;

    sp = D_neo_ark_shrine_8017D5D0;
    sp.funcs[task->state](task);
}
