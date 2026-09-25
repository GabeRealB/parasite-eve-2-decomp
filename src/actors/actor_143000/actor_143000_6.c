#include "common.h"

#include "actors/actor_143000.h"

#include "main/task.h"

/// Callback of the action-prompt task that `func_actor_143000_801324C8` spawns
/// from `D_actor_143000_80134558`: a two-state dispatcher whose handler table
/// is built on the stack. State 0, `func_actor_143000_80133C90`, resets both
/// prompt slots; state 1, `func_actor_143000_80131F80`, drives the cursor every
/// frame from then on.
void func_actor_143000_80133578(Task* task)
{
    TaskFunc funcs[2] = {
        func_actor_143000_80133C90,
        func_actor_143000_80131F80,
    };

    funcs[task->state](task);
}
