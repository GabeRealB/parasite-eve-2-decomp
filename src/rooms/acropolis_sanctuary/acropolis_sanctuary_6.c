#include "common.h"

#include "main/task.h"

extern const TaskFuncTable3 D_acropolis_sanctuary_8017D5C4;

/// Runs the room task's current state through a stack copy of the room's
/// three-entry state table: set-up, the per-frame entry fixup and `taskKill`.
void func_acropolis_sanctuary_8017D9E8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_sanctuary_8017D5C4;
    sp.funcs[task->state](task);
}
