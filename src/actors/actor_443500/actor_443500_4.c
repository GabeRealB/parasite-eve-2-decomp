#include "common.h"

#include "main/task.h"

/// State table of the actor's main task (`TaskDesc` entry 0): the spawn
/// handler, the per-frame tick and the exit callback.
extern TaskFuncTable3 D_actor_443500_80131E30;

/// Global mode byte in the main executable; while it is nonzero the main task
/// skips its state handler.
extern u8 D_801153F4;

/// Per-frame dispatcher of the main task: runs its spawn, tick or exit state
/// from `D_actor_443500_80131E30`, skipping the frame while `D_801153F4` is
/// set.
void func_actor_443500_80132738(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_443500_80131E30;
    if (D_801153F4 == 0) {
        sp.funcs[task->state](task);
    }
}
