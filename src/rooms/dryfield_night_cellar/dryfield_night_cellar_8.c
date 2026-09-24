#include "common.h"

#include "main/task.h"

/// The room entry task's three states: set the room up, idle, end.
extern const TaskFuncTable3 D_dryfield_night_cellar_8017D5C4;

/// Runs the room entry task's current state from its three-entry table, which
/// it copies onto the stack before the call.
void func_dryfield_night_cellar_8017D748(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_cellar_8017D5C4;
    sp.funcs[task->state](task);
}
