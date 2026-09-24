#include "common.h"

#include "main/task.h"

extern const TaskFuncTable3 D_acropolis_east_elevator_hall_8017D5D4;

/// Runs the room task's current state through a stack copy of the room's
/// three-entry state table.
void func_acropolis_east_elevator_hall_8017F55C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_east_elevator_hall_8017D5D4;
    sp.funcs[task->state](task);
}
