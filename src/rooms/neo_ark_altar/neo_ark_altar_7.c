#include "common.h"

#include "main/task.h"

/// State table of the room's message task: set-up
/// (`func_neo_ark_altar_8017D974`), an empty per-frame state and `taskKill`.
extern const TaskFuncTable3 D_neo_ark_altar_8017D5C4;

/// Runs the room's message task's current state through a stack copy of
/// `D_neo_ark_altar_8017D5C4`.
void func_neo_ark_altar_8017D9E8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_altar_8017D5C4;
    sp.funcs[task->state](task);
}
