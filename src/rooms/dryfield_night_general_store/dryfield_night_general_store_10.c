#include "common.h"

#include "main/task.h"

extern const TaskFuncTable3 D_dryfield_night_general_store_8017D5F4;

/// The room task: runs the state `D_dryfield_night_general_store_8017D5F4`
/// names for `task->state`, through a stack copy of the table.
void func_dryfield_night_general_store_8017DE88(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_general_store_8017D5F4;
    sp.funcs[task->state](task);
}
