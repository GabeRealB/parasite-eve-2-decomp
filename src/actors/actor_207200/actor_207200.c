#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_207200_80149E24;

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200", func_actor_207200_80149E84);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200", func_actor_207200_8014A1C4);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200", func_actor_207200_8014A588);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200", func_actor_207200_8014AA74);

INCLUDE_RODATA("actors/nonmatchings/actor_207200/actor_207200", D_actor_207200_80149E20);

INCLUDE_RODATA("actors/nonmatchings/actor_207200/actor_207200", D_actor_207200_80149E24);

void func_actor_207200_8014AC9C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_207200_80149E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200", func_actor_207200_8014ACF8);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200", func_actor_207200_8014AE08);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200", func_actor_207200_8014AE70);

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200", func_actor_207200_8014AF2C);

/// Colours the actor from the *second* attach coordinate of its model: takes a
/// 0x10-byte `VECTOR` off `G_SCRATCH_HEAD`, fills it with that coordinate's
/// world position and hands it to `Gp_UpdateActorColor` with no blend
/// parameters. `arg0` is the colour target, passed straight through.
void func_actor_207200_8014AFDC(void* arg0, Task* task)
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

INCLUDE_RODATA("actors/nonmatchings/actor_207200/actor_207200", D_actor_207200_80149E30);
