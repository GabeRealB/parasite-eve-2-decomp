#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_g_r_kitchen.h"

/// The room task: runs the state the task is in from a stack copy of the
/// room's three-state table.
void func_dryfield_g_r_kitchen_8017D9A4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_g_r_kitchen_8017D5DC;
    sp.funcs[task->state](task);
}
