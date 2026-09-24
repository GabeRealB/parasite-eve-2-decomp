#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_207200.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// Draws the enemy's ground quad under its model root, at the translation of
/// the root part's `workm`, staged in a `VECTOR3` on the scratch stack.
void func_actor_207200_8014D77C(Task* task)
{
    GsCOORDINATE2* coord;
    VECTOR3*       vec;

    coord   = ((TmdObject*)task->extra)->coords;
    vec     = (VECTOR3*)(SCRATCH_SP -= 0x18);
    vec->vx = coord->workm.t[0];
    vec->vy = coord->workm.t[1];
    vec->vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(vec, 0x1C0, 0);
    SCRATCH_SP += 0x18;
}
