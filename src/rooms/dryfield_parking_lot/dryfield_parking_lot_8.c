#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_parking_lot.h"

/// The room entry task: copies the three-state table to the stack and runs the
/// entry the task's state selects.
void func_dryfield_parking_lot_8017DB54(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_parking_lot_8017D5DC;
    sp.funcs[task->state](task);
}
