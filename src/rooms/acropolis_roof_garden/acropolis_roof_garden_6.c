#include "common.h"

#include "main/task.h"

extern const TaskFuncTable3 D_acropolis_roof_garden_8017D5C4;

/// Runs the task's current state through a stack copy of the room's
/// three-entry state table.
void func_acropolis_roof_garden_8017DC74(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_roof_garden_8017D5C4;
    sp.funcs[task->state](task);
}
