#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

void Gp_UpdateActorColor(void* arg0, VECTOR* arg1, s32 arg2, s32 arg3);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80131E68);

INCLUDE_RODATA("actors/nonmatchings/actor_104600/actor_104600", D_actor_104600_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_104600/actor_104600", D_actor_104600_80131E24);

INCLUDE_RODATA("actors/nonmatchings/actor_104600/actor_104600", D_actor_104600_80131E30);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_801321F4);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_801323D0);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_801325D0);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80132798);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80132CE8);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80132DF8);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80132F30);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80133424);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_801335EC);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_8013391C);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80133C2C);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80133D74);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_801342C4);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80134320);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80134438);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_8013454C);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_801345DC);

/// Colours the actor from the *second* attach coordinate of its model: takes a
/// 0x10-byte `VECTOR` off `G_SCRATCH_HEAD`, fills it with that coordinate's
/// world position and hands it to `Gp_UpdateActorColor` with no blend
/// parameters. `arg0` is the colour target, passed straight through.
void func_actor_104600_80134690(void* arg0, Task* task)
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

INCLUDE_RODATA("actors/nonmatchings/actor_104600/actor_104600", D_actor_104600_80131E5C);
