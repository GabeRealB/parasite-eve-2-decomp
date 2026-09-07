#include "common.h"

#include "main/task.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_310600_80161E24;

INCLUDE_ASM("actors/nonmatchings/actor_310600/actor_310600", func_actor_310600_80161E64);

INCLUDE_ASM("actors/nonmatchings/actor_310600/actor_310600", func_actor_310600_80161FA0);

INCLUDE_ASM("actors/nonmatchings/actor_310600/actor_310600", func_actor_310600_8016231C);

INCLUDE_ASM("actors/nonmatchings/actor_310600/actor_310600", func_actor_310600_8016246C);

INCLUDE_ASM("actors/nonmatchings/actor_310600/actor_310600", func_actor_310600_801625F0);

INCLUDE_RODATA("actors/nonmatchings/actor_310600/actor_310600", D_actor_310600_80161E20);

INCLUDE_RODATA("actors/nonmatchings/actor_310600/actor_310600", D_actor_310600_80161E24);

void func_actor_310600_8016274C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_310600_80161E24;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_310600/actor_310600", func_actor_310600_801627A4);

INCLUDE_ASM("actors/nonmatchings/actor_310600/actor_310600", func_actor_310600_801628B0);

INCLUDE_RODATA("actors/nonmatchings/actor_310600/actor_310600", D_actor_310600_80161E3C);

INCLUDE_RODATA("actors/nonmatchings/actor_310600/actor_310600", D_actor_310600_80161E48);

INCLUDE_RODATA("actors/nonmatchings/actor_310600/actor_310600", D_actor_310600_80161E54);
