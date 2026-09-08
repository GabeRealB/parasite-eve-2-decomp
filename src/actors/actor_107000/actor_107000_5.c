#include "common.h"

#include "main/task.h"

#include "gameplay/1BC.h"

/// The enemy's four main-body handlers, dispatched through by state. Two
/// separate state machines in this overlay run the same dispatch shape over
/// their own table.
extern GpEnemyTaskFuncTable4 D_actor_107000_80131E30;
extern GpEnemyTaskFuncTable4 D_actor_107000_80131E5C;

void func_actor_107000_80134B30(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;

    sp = D_actor_107000_80131E30;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80134B98);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80134C2C);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80134F84);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80135280);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_8013560C);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80135C28);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80136094);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80136288);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_801364D8);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80136614);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_801367E0);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80136938);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80136C80);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80136E88);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80137220);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_8013777C);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_801378D8);

void func_actor_107000_80137C8C(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;

    sp = D_actor_107000_80131E5C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80137CF4);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80137DA4);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80137E18);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80137EA8);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_80137F1C);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_5", func_actor_107000_801380C8);
