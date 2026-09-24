#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_saloon_g_r.h"

/// The room task: runs the state the task is in from a stack copy of the
/// room's three-state table.
void func_dryfield_saloon_g_r_8017DA18(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_saloon_g_r_8017D5DC;
    sp.funcs[task->state](task);
}
