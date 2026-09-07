#include "common.h"

#include "main/task.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_213100_80149E24;

INCLUDE_ASM("actors/nonmatchings/actor_213100/actor_213100", func_actor_213100_80149E3C);

INCLUDE_RODATA("actors/nonmatchings/actor_213100/actor_213100", D_actor_213100_80149E20);

INCLUDE_RODATA("actors/nonmatchings/actor_213100/actor_213100", D_actor_213100_80149E24);

void func_actor_213100_80149FE4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213100_80149E24;
    sp.funcs[task->state](task);
}

INCLUDE_RODATA("actors/nonmatchings/actor_213100/actor_213100", D_actor_213100_80149E30);
