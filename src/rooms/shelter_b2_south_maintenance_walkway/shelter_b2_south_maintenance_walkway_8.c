#include "common.h"

#include "main/task.h"

/// The room task's three states: setup, idle and exit.
extern const TaskFuncTable3 D_shelter_b2_south_maintenance_walkway_8017D5F0;

/// Runs the room task's current state from its state table, dispatching
/// through a copy of the table taken onto the stack.
void func_shelter_b2_south_maintenance_walkway_8017DC6C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_south_maintenance_walkway_8017D5F0;
    sp.funcs[task->state](task);
}
