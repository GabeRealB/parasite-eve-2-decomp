#include "common.h"

#include "main/task.h"

#include "actors/actor_400500.h"

/// Per-frame entry point of the actor's task: runs the handler its state
/// selects from `D_actor_400500_80131E4C` - set-up, the per-frame state
/// machine, the death sequence and the post-death timeout. The table is a
/// local, so GCC copies it from `.rodata` onto the stack every frame.
void func_actor_400500_8013DE98(Task* task)
{
    TaskFuncTable4 states;

    states = D_actor_400500_80131E4C;
    states.funcs[task->state](task);
}
