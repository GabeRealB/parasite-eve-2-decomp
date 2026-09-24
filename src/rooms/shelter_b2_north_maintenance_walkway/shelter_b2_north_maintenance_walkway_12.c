#include "common.h"

#include "main/task.h"

/// The room task's three states: setup, idle and exit.
extern const TaskFuncTable3 D_shelter_b2_north_maintenance_walkway_8017D5F4;

/// Runs the room task's current state from its state table, dispatching
/// through a copy of the table taken onto the stack.
void func_shelter_b2_north_maintenance_walkway_8017DD90(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_north_maintenance_walkway_8017D5F4;
    sp.funcs[task->state](task);
}
