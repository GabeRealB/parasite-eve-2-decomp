#include "common.h"

#include "main/task.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_135600_80131E24;

INCLUDE_ASM("actors/nonmatchings/actor_135600/actor_135600", func_actor_135600_80131E68);

INCLUDE_ASM("actors/nonmatchings/actor_135600/actor_135600", func_actor_135600_80132234);

INCLUDE_ASM("actors/nonmatchings/actor_135600/actor_135600", func_actor_135600_801324D0);

INCLUDE_ASM("actors/nonmatchings/actor_135600/actor_135600", func_actor_135600_801326E8);

INCLUDE_ASM("actors/nonmatchings/actor_135600/actor_135600", func_actor_135600_8013282C);

INCLUDE_RODATA("actors/nonmatchings/actor_135600/actor_135600", D_actor_135600_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_135600/actor_135600", D_actor_135600_80131E24);

void func_actor_135600_801329E0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135600_80131E24;
    sp.funcs[task->state](task);
}

INCLUDE_RODATA("actors/nonmatchings/actor_135600/actor_135600", D_actor_135600_80131E30);

INCLUDE_RODATA("actors/nonmatchings/actor_135600/actor_135600", D_actor_135600_80131E3C);
