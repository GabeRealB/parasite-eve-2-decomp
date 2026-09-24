#include "common.h"

#include "main/task.h"

extern const TaskFuncTable3 D_acropolis_hallway_8017D5C4;

/// Runs the room task's current state through a stack copy of the room's
/// three-entry state table.
void func_acropolis_hallway_8017D7D0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_hallway_8017D5C4;
    sp.funcs[task->state](task);
}
