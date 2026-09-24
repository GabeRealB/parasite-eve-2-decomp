#include "common.h"

#include "main/task.h"

#include "rooms/dryfield_back_street.h"

/// Runs the room entry task's current state from its three-entry table, which
/// it copies onto the stack before the call.
void func_dryfield_back_street_8017D918(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_back_street_8017D5C4;
    sp.funcs[task->state](task);
}
