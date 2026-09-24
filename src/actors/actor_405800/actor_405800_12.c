#include "common.h"

#include "main/task.h"

#include "actors/actor_405800.h"

/// Per-frame entry point of the actor's task: runs whichever of the four
/// handlers in `D_actor_405800_80131E54` the task's `state` selects. The table
/// is a local, so GCC copies it from `.rodata` onto the stack every frame.
void func_actor_405800_80138634(Task* task)
{
    TaskFuncTable4 states;

    states = D_actor_405800_80131E54;
    states.funcs[task->state](task);
}
