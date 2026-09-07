#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

void Gp_UpdateActorColor(void* arg0, VECTOR* arg1, s32 arg2, s32 arg3);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_80149E68);

INCLUDE_RODATA("actors/nonmatchings/actor_204600/actor_204600", D_actor_204600_80149E20);

INCLUDE_RODATA("actors/nonmatchings/actor_204600/actor_204600", D_actor_204600_80149E24);

INCLUDE_RODATA("actors/nonmatchings/actor_204600/actor_204600", D_actor_204600_80149E30);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014A1F4);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014A3D0);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014A5D0);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014A798);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014ACE8);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014ADF8);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014AF30);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014B424);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014B5EC);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014B91C);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014BC2C);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014BD74);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014C2C4);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014C320);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014C438);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014C54C);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014C5DC);

/// Colours the actor from the *second* attach coordinate of its model: takes a
/// 0x10-byte `VECTOR` off `G_SCRATCH_HEAD`, fills it with that coordinate's
/// world position and hands it to `Gp_UpdateActorColor` with no blend
/// parameters. `arg0` is the colour target, passed straight through.
void func_actor_204600_8014C690(void* arg0, Task* task)
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

INCLUDE_RODATA("actors/nonmatchings/actor_204600/actor_204600", D_actor_204600_80149E5C);
