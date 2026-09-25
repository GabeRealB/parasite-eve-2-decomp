#include "common.h"

#include "main/task.h"

#include "actors/actor_800300.h"

/// State handlers of the actor's main task, indexed by its state: set-up, the
/// per-frame update, a step that only advances to the last state, and the
/// teardown.
extern TaskFuncTable4 D_actor_800300_80161E24;

/// Per-frame entry point of the actor's main task: runs the handler its state
/// selects. The table is a local, so it is copied from `.rodata` onto the
/// stack on every call.
void func_actor_800300_801625F4(Task* task)
{
    TaskFuncTable4 states;

    states = D_actor_800300_80161E24;
    states.funcs[task->state](task);
}
