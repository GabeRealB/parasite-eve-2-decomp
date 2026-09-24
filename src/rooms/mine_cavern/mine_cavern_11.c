#include "common.h"

#include "main/task.h"
#include "rooms/mine_cavern.h"

/// Runs the room task's current state handler from the room's three-entry
/// table, copying the table onto the stack before the call.
void func_mine_cavern_8017DF54(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mine_cavern_8017D5C4;
    sp.funcs[task->state](task);
}
