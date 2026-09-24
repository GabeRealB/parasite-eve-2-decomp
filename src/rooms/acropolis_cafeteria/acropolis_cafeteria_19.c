#include "common.h"

#include "main/task.h"

extern TaskFuncTable4 D_acropolis_cafeteria_8017D69C;

/// Runs the task's current state through a stack copy of the room's
/// four-entry state table.
void func_acropolis_cafeteria_80181E70(Task* task)
{
    TaskFuncTable4 states;

    states = D_acropolis_cafeteria_8017D69C;
    states.funcs[task->state](task);
}
