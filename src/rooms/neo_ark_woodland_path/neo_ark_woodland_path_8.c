#include "common.h"

#include "main/task.h"
#include "rooms/neo_ark_woodland_path.h"

/// Task tick that dispatches on the task's state through the three-entry
/// handler table `D_neo_ark_woodland_path_8017D614`, copied to the stack first.
void func_neo_ark_woodland_path_8017E9B0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_woodland_path_8017D614;
    sp.funcs[task->state](task);
}
