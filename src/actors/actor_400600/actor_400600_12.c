#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/3A34.h"

#include "actors/actor_400600.h"

/// Colours the actor from its model's second coordinate: takes a 0x10-byte
/// `VECTOR` off `G_SCRATCH_HEAD`, fills it with that coordinate's world
/// position and hands it to `Gp_UpdateActorColor` for the task's `spawnArg2`,
/// with no blend parameters.
void func_actor_400600_8013A2C0(Task* task)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;

    coord     = &((TmdObject*)task->extra)->coords[1];
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    Gp_UpdateActorColor(task->spawnArg2, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}
