#include "common.h"

#include "main/task.h"

/// State handlers of `func_dryfield_night_garage_801803BC`.
extern const TaskFuncTable3 D_dryfield_night_garage_8017D6FC;

/// Runs the handler for the task's state from a stack copy of
/// `D_dryfield_night_garage_8017D6FC`.
void func_dryfield_night_garage_801803BC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_garage_8017D6FC;
    sp.funcs[task->state](task);
}
