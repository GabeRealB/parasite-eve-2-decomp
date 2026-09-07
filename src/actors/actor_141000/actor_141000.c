#include "common.h"

#include "main/task.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_141000_80131E30;

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000", func_actor_141000_80131E94);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000", func_actor_141000_801323F0);

INCLUDE_RODATA("actors/nonmatchings/actor_141000/actor_141000", D_actor_141000_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_141000/actor_141000", D_actor_141000_80131E24);

INCLUDE_RODATA("actors/nonmatchings/actor_141000/actor_141000", D_actor_141000_80131E30);

void func_actor_141000_80132C24(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_141000_80131E30;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000", func_actor_141000_80132C7C);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000", func_actor_141000_80132D3C);
