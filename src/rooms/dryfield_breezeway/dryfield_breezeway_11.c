#include "common.h"

#include "main/task.h"

#include "rooms/dryfield_breezeway.h"

/// The room's prompt task, run from `D_dryfield_breezeway_80182DC0`: state 0
/// resets both action-prompt slots (`func_dryfield_breezeway_8017FF1C`), state
/// 1 drives the cursor every frame after that
/// (`func_dryfield_breezeway_8017F538`). The handler pair is built on the stack
/// rather than read from rodata.
void func_dryfield_breezeway_8017FA80(Task* task)
{
    TaskFunc states[2] = { func_dryfield_breezeway_8017FF1C, func_dryfield_breezeway_8017F538 };

    states[task->state](task);
}
