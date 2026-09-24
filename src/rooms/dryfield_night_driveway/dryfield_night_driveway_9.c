#include "common.h"

#include "main/task.h"

/// The room task's three states: set up, idle, kill.
extern const TaskFuncTable3 D_dryfield_night_driveway_8017D5D8;

/// Room task: copies the state table onto the stack and runs the entry for the
/// task's current state.
void func_dryfield_night_driveway_8017DD8C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_driveway_8017D5D8;
    sp.funcs[task->state](task);
}
