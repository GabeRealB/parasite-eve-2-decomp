#include "common.h"

#include "main/task.h"

#include "rooms/dryfield_general_store.h"

/// The room task: runs the state `D_dryfield_general_store_8017D5F4` names for
/// `task->state`, through a stack copy of the table.
void func_dryfield_general_store_8017DF5C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_general_store_8017D5F4;
    sp.funcs[task->state](task);
}
