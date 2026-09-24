#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_main_street.h"

/// The room entry task: runs the state of the three-entry table the task is in.
void func_dryfield_main_street_8017E168(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_main_street_8017D5F4;
    sp.funcs[task->state](task);
}
