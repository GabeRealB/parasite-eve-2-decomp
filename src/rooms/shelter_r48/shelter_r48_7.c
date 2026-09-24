#include "common.h"

#include "main/task.h"
#include "rooms/shelter_r48.h"

/// Runs one tick of the room task through the three-state table
/// `D_shelter_r48_8017D608`, copying the table onto the stack and calling the
/// entry for the task's current state.
void func_shelter_r48_8017E224(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_r48_8017D608;
    sp.funcs[task->state](task);
}
