#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_night_main_street.h"

/// Runs the room task's current state from its three-entry table, which it
/// copies onto the stack before the call.
void func_dryfield_night_main_street_8017E0C0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_main_street_8017D5F4;
    sp.funcs[task->state](task);
}
