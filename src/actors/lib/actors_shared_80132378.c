#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// Draws the actor's ground shadow quad under the model root, skipping actors
/// that are pending a deferred kill (`field_C & 0x80`) or that have no aux
/// buffer yet. The world position is the translation of the root part's
/// `workm`, staged in a scratchpad VECTOR3 rather than on the stack.
void ActorsShared80132378(Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR3*       vec;

    obj   = (TmdObject*)task->extra;
    coord = obj->field_8;
    if (!(obj->field_C & 0x80) && obj->field_18 != NULL) {
        vec     = (VECTOR3*)(SCRATCH_SP -= 0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, 0xC0);
        SCRATCH_SP += 0x18;
    }
}
