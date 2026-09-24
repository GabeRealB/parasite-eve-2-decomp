#include "common.h"

#include "main/task.h"

extern const TaskFuncTable3 D_dryfield_driveway_8017D5D8;

/// The room task: dispatches through its three-state table, copied onto the
/// stack first.
void func_dryfield_driveway_8017DE14(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_driveway_8017D5D8;
    sp.funcs[task->state](task);
}
