#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/gameplay.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_511000_80131E30;
extern TaskFuncTable3 D_actor_511000_80131E3C;

void func_actor_511000_80132224(void)
{
}

void func_actor_511000_8013222C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E30;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_2", func_actor_511000_80132284);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_2", func_actor_511000_80132390);

void func_actor_511000_80132428(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E3C;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_2", func_actor_511000_80132480);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_2", func_actor_511000_801325A4);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_2", func_actor_511000_80132604);

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000_2", D_actor_511000_80131E48);

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000_2", D_actor_511000_80131E54);

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000_2", D_actor_511000_80131E60);

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000_2", ActorsShared80135df4Table);
