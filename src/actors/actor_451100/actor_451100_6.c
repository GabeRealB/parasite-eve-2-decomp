#include "common.h"

#include "actors/actor_451100.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

void func_actor_451100_80132CD4(Task* task);

/// Refreshes the model root's coordinate, feeds its world translation (raised
/// by 800 on y) to `func_800D7A9C`, then runs the animation starter and
/// `func_actor_451100_80132CD4`, which draws a ground quad.
void func_actor_451100_80132C28(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         pos;

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 800;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_451100_80132A1C(task);
    func_actor_451100_80132CD4(task);
}

void func_actor_451100_80132CAC(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Draws the ground shadow quad under the model root of the actor
/// `func_actor_451100_80132BD4` dispatches, unless the model is hidden
/// (`flags & 0x80`) or has no buffer yet; the same body as
/// `func_actor_451100_8013280C`.
void func_actor_451100_80132CD4(Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR3*       vec;

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    if (!(obj->flags & 0x80) && obj->buffer != NULL) {
        vec     = (VECTOR3*)(SCRATCH_SP -= 0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShade);
        SCRATCH_SP += 0x18;
    }
}
