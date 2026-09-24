#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_motel_balcony.h"

/// Runs the room task's current state from its state table, dispatching
/// through a copy of the table taken onto the stack.
void func_dryfield_motel_balcony_8017DBD0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_motel_balcony_8017D5DC;
    sp.funcs[task->state](task);
}
