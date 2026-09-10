#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_207200_80149E30;

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014B278);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014B628);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014B87C);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014BEF4);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014C870);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014CA84);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014CE20);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014CFEC);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014D128);

void func_actor_207200_8014D280(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_207200_80149E30;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014D2DC);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014D41C);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014D49C);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014D5C4);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_3", func_actor_207200_8014D65C);

/// Colours the actor from the *second* attach coordinate of its model: takes a
/// 0x10-byte `VECTOR` off `G_SCRATCH_HEAD`, fills it with that coordinate's
/// world position and hands it to `Gp_UpdateActorColor` with no blend
/// parameters. `arg0` is the colour target, passed straight through.
void func_actor_207200_8014D70C(void* arg0, Task* task)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;

    coord     = &((TmdObject*)task->extra)->field_8[1];
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    Gp_UpdateActorColor(arg0, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}
