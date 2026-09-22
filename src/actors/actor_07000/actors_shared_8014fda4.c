#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/3A34.h"

/// Colours the model from the *second* attach coordinate: takes a 0x10-byte
/// `VECTOR` off `G_SCRATCH_HEAD`, fills it with that coordinate's world position
/// and hands it to `Gp_UpdateActorColor` with no blend parameters. The colour
/// target is the task's own `spawnArg2`, so it is read first.
void ActorsShared8014fda4(Task* task)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;
    void*          obj;

    obj       = task->spawnArg2;
    coord     = &((TmdObject*)task->extra)->coords[1];
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    Gp_UpdateActorColor(obj, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}
