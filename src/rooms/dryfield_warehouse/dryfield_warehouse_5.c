#include "common.h"

#include "main/task.h"

#include "rooms/dryfield_warehouse.h"

/// The three states of the room's main task: set-up, an idle per-frame step
/// and the kill.
extern const TaskFuncTable3 D_dryfield_warehouse_8017D5C4;

/// Dispatches the room's main task through its three-state table, copied onto
/// the stack first.
void func_dryfield_warehouse_8017DA00(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_warehouse_8017D5C4;
    sp.funcs[task->state](task);
}
