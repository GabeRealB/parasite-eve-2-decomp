#include "common.h"
#include "actors/actor_213100.h"

#include "main/task.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_213100_80149E30;

void func_actor_213100_8014A0B8(void)
{
}

void func_actor_213100_8014A0C0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213100_80149E30;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_213100/actor_213100_2", func_actor_213100_8014A118);
