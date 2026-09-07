#include "common.h"

#include "main/task.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_135600_80131E30;
extern TaskFuncTable3 D_actor_135600_80131E3C;

void func_actor_135600_80132AB4(void)
{
}

void func_actor_135600_80132ABC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135600_80131E30;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_135600/actor_135600_2", func_actor_135600_80132B14);

INCLUDE_ASM("actors/nonmatchings/actor_135600/actor_135600_2", func_actor_135600_80132C18);

INCLUDE_ASM("actors/nonmatchings/actor_135600/actor_135600_2", func_actor_135600_80132C80);

void func_actor_135600_80132D64(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135600_80131E3C;
    sp.funcs[task->state](task);
}

INCLUDE_RODATA("actors/nonmatchings/actor_135600/actor_135600_2", D_actor_135600_80131E48);

INCLUDE_RODATA("actors/nonmatchings/actor_135600/actor_135600_2", ActorsShared80132920Offset);
