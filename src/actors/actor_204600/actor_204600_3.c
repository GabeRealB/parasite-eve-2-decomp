#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

void Gp_UpdateActorColor(void* arg0, VECTOR* arg1, s32 arg2, s32 arg3);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_3", func_actor_204600_8014CA8C);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_3", func_actor_204600_8014CAF4);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_3", func_actor_204600_8014CB88);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_3", func_actor_204600_8014CEC8);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_3", func_actor_204600_8014D28C);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_3", func_actor_204600_8014D778);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_3", func_actor_204600_8014D9A0);

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
