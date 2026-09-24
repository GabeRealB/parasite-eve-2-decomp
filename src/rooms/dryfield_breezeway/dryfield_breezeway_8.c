#include "common.h"

#include "main/task.h"

#include "rooms/dryfield_breezeway.h"

/// The message task's three state handlers, in the room's leading rodata.
extern const TaskFuncTable3 D_dryfield_breezeway_8017D5DC;

/// Runs the room's message task through its three states: publishing the
/// room's message table (`func_dryfield_breezeway_8017DDB0`), idling
/// (`func_dryfield_breezeway_8017DE60`) and `taskKill`. The table is copied
/// onto the stack first, so the call goes through a local copy rather than the
/// rodata.
void func_dryfield_breezeway_8017DE68(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_breezeway_8017D5DC;
    sp.funcs[task->state](task);
}
