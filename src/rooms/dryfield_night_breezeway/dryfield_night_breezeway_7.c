#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_night_breezeway.h"

/// The room's event task: copies the three-state table onto the stack and
/// calls the entry for the task's current state.
void func_dryfield_night_breezeway_8017D680(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_breezeway_8017D5C4;
    sp.funcs[task->state](task);
}
