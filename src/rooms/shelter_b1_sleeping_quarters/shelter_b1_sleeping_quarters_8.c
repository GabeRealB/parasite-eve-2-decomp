#include "common.h"

#include "main/task.h"
#include "rooms/shelter_b1_sleeping_quarters.h"

/// Runs the room task's current state from a stack copy of its state table.
void func_shelter_b1_sleeping_quarters_8017D888(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_sleeping_quarters_8017D5C4;
    sp.funcs[task->state](task);
}
