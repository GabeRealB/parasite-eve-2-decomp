#include "common.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "actors/actors_shared_801388e8.h"

/// Squared distance from `arg0` to the slot-3 (player) task's root part coord,
/// or `0x7FFFFFFF` when that task is gone. The delta is staged in an `SVECTOR`
/// carved off the scratchpad stack and squared with `Gfx_ApplyMatrixNoSf`.
s32 ActorsShared801388e8(GsCOORDINATE2* arg0)
{
    void**         scratch;
    u8*            head;
    SVECTOR*       vec;
    GsCOORDINATE2* coord;
    Task*          task;
    s32            ret;

    task = (Task*)Game_GetPtrSlot(3);
    if (task != NULL) {
        coord    = ((TmdObject*)task->extra)->field_8;
        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        vec      = (SVECTOR*)(head - 8);
        vec->vx  = (u16)coord->workm.t[0] - (u16)arg0->workm.t[0];
        vec->vy  = (u16)coord->workm.t[1] - (u16)arg0->workm.t[1];
        *scratch = vec;
        vec->vz  = (u16)coord->workm.t[2] - (u16)arg0->workm.t[2];
        ret      = Gfx_ApplyMatrixNoSf(vec, vec);
        *scratch = (u8*)*scratch + 8;
    } else {
        ret = 0x7FFFFFFF;
    }
    return ret;
}
