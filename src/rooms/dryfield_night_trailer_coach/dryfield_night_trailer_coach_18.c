#include "common.h"

#include "main/task.h"

/// State handlers of `func_dryfield_night_trailer_coach_801828CC`.
extern const TaskFuncTable3 D_dryfield_night_trailer_coach_8017D7DC;

/// Runs the handler for the task's state from a stack copy of
/// `D_dryfield_night_trailer_coach_8017D7DC`.
void func_dryfield_night_trailer_coach_801828CC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_trailer_coach_8017D7DC;
    sp.funcs[task->state](task);
}
