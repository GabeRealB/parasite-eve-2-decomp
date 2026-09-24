#include "common.h"

#include "main/task.h"
#include "rooms/neo_ark_bridge.h"

/// Task tick that dispatches on the task's state through the three-entry
/// handler table `D_neo_ark_bridge_8017D614`, copied to the stack first.
void func_neo_ark_bridge_8017E8FC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_bridge_8017D614;
    sp.funcs[task->state](task);
}
