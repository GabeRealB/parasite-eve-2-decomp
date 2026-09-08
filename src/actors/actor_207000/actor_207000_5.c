#include "common.h"

#include "main/task.h"

#include "gameplay/1BC.h"

/// The enemy's four main-body handlers, dispatched through by state. Two
/// separate state machines in this overlay run the same dispatch shape over
/// their own table.
extern GpEnemyTaskFuncTable4 D_actor_207000_80149E30;
extern GpEnemyTaskFuncTable4 D_actor_207000_80149E5C;

void func_actor_207000_8014CB30(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;

    sp = D_actor_207000_80149E30;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014CB98);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014CC2C);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014CF84);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014D280);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014D60C);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014DC28);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014E094);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014E288);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014E4D8);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014E614);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014E7E0);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014E938);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014EC80);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014EE88);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014F220);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014F77C);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014F8D8);

void func_actor_207000_8014FC8C(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;

    sp = D_actor_207000_80149E5C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014FCF4);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014FDA4);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014FE18);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014FEA8);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014FF1C);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_801500C8);
