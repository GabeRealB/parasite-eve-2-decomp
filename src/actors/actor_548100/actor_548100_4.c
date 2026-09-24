#include "common.h"

#include "actors/actor_548100.h"

#include "main/task.h"

/// Callback of the action-prompt task: a two-state dispatcher whose handler
/// table is built on the stack. State 0, `func_actor_548100_80135154`, resets
/// both prompt slots; state 1, `func_actor_548100_80131ED8`, drives the cursor
/// every frame from then on.
void func_actor_548100_80134728(Task* task)
{
    TaskFunc funcs[2] = {
        func_actor_548100_80135154,
        func_actor_548100_80131ED8,
    };

    funcs[task->state](task);
}
