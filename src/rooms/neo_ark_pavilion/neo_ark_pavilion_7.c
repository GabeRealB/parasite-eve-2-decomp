#include "common.h"

#include "main/task.h"

extern const TaskFuncTable3 D_neo_ark_pavilion_8017D628;

/// Task tick that dispatches on the task's state through the three-entry
/// handler table `D_neo_ark_pavilion_8017D628`, copied to the stack first.
void func_neo_ark_pavilion_8017EBF4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_pavilion_8017D628;
    sp.funcs[task->state](task);
}
