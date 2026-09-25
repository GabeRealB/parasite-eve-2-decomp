#include "common.h"

#include "actors/actor_420700.h"

/// Task handler of the actor: republishes the task's work block in
/// `D_actor_420700_8013EFE0`, so the rest of the overlay can reach it without
/// the task, then runs the handler for the task's state from a two-entry table
/// built on the stack -- the spawn step `func_actor_420700_80131E24` or the
/// per-frame step `func_actor_420700_80132064`.
void func_actor_420700_80132340(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_420700_80131E24,
        func_actor_420700_80132064,
    };

    D_actor_420700_8013EFE0 = task->work;
    fns[task->state](task->spawnArg2, task);
}
