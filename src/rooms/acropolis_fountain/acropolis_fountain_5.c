#include "common.h"

#include "main/task.h"

/// State handlers of the room task: set-up, an empty per-frame tick and
/// `taskKill`.
extern const TaskFuncTable3 D_acropolis_fountain_8017D5C4;

/// Runs the room task's current state through a stack copy of its three-entry
/// state table.
void func_acropolis_fountain_8017D9C4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_fountain_8017D5C4;
    sp.funcs[task->state](task);
}
