#include "common.h"

#include "main/task.h"

#include "rooms/dryfield_breezeway.h"

/// The key-item event task's seven state handlers, in the room's leading
/// rodata.
extern const TaskFuncTable7 D_dryfield_breezeway_8017D5E8;

/// The room's key-item event task, run from `D_dryfield_breezeway_80182E18`:
/// dispatches the current state through the seven handlers of
/// `D_dryfield_breezeway_8017D5E8`, copied onto the stack first so the call
/// goes through a local table rather than through `.rodata`.
void func_dryfield_breezeway_8017FC38(Task* task)
{
    TaskFuncTable7 sp;

    sp = D_dryfield_breezeway_8017D5E8;
    sp.funcs[task->state](task);
}
