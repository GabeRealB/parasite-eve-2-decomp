#include "common.h"

#include "main/task.h"

#include "actors/actor_800100.h"

/// Per-frame entry point of the actor's main task: runs the handler its state
/// selects from the four-entry state table - set-up, the per-frame update, a
/// step that only advances to the last state, and the teardown that kills the
/// actor's child tasks and unlinks its objects. The table is a local, so it is
/// copied from `.rodata` onto the stack on every call.
void func_actor_800100_80163CF0(Task* task)
{
    TaskFuncTable4 states;

    states = D_actor_800100_80161E3C;
    states.funcs[task->state](task);
}
