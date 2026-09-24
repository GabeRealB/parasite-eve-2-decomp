#include "common.h"

#include "main/task.h"

#include "rooms/shelter_b4_reservoir.h"

/// Runs a task through the room's three-entry state table
/// `D_shelter_b4_reservoir_8017D5C4`, copied onto the stack first.
void func_shelter_b4_reservoir_8017E88C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b4_reservoir_8017D5C4;
    sp.funcs[task->state](task);
}
