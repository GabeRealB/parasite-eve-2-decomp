#include "common.h"

#include "main/task.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_511000_80131E24;

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000", func_actor_511000_80131E78);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000", func_actor_511000_80132048);

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000", D_actor_511000_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000", D_actor_511000_80131E24);

void func_actor_511000_80132150(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E24;
    sp.funcs[task->state](task);
}

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000", D_actor_511000_80131E30);

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000", D_actor_511000_80131E3C);
