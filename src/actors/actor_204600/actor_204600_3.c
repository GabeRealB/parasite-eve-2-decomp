#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_204600_80149E5C;

/// The enemy's four main-body handlers, dispatched through by state.
extern GpEnemyTaskFuncTable4 D_actor_204600_80149E30;

void func_actor_204600_8014CA8C(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;

    sp = D_actor_204600_80149E30;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_3", func_actor_204600_8014CAF4);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_3", func_actor_204600_8014CB88);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_3", func_actor_204600_8014CEC8);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_3", func_actor_204600_8014D28C);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_3", func_actor_204600_8014D778);

void func_actor_204600_8014D9A0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_204600_80149E5C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_3", func_actor_204600_8014D9FC);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_3", func_actor_204600_8014DB0C);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_3", func_actor_204600_8014DB74);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_3", func_actor_204600_8014DC30);

/// Colours the actor from the *second* attach coordinate of its model: takes a
/// 0x10-byte `VECTOR` off `G_SCRATCH_HEAD`, fills it with that coordinate's
/// world position and hands it to `Gp_UpdateActorColor` with no blend
/// parameters. `arg0` is the colour target, passed straight through.
void func_actor_204600_8014DCE0(void* arg0, Task* task)
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

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_3", func_actor_204600_8014DD50);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_3", func_actor_204600_8014DE2C);
