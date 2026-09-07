#include "common.h"

#include "main/task.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_213000_80149E24;

INCLUDE_ASM("actors/nonmatchings/actor_213000/actor_213000", func_actor_213000_80149E54);

INCLUDE_RODATA("actors/nonmatchings/actor_213000/actor_213000", D_actor_213000_80149E20);

INCLUDE_RODATA("actors/nonmatchings/actor_213000/actor_213000", D_actor_213000_80149E24);

void func_actor_213000_8014A084(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E24;
    sp.funcs[task->state](task);
}

INCLUDE_RODATA("actors/nonmatchings/actor_213000/actor_213000", D_actor_213000_80149E30);

INCLUDE_RODATA("actors/nonmatchings/actor_213000/actor_213000", D_actor_213000_80149E3C);

INCLUDE_RODATA("actors/nonmatchings/actor_213000/actor_213000", D_actor_213000_80149E48);
