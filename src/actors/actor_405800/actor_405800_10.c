#include "common.h"

#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actor_405800.h"

/// Refreshes the view coordinate and coordinate `index` of the actor's model,
/// then stores that coordinate's view-space X and Z translation to `out`; `y`
/// is left untouched. Every caller passes the work block's `field_88`.
void func_actor_405800_80138478(Task* task, s16 index, Actor405800ViewPos* out)
{
    MATRIX         local;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* coords;

    coords            = (GsCOORDINATE2*)((TmdObject*)task->extra)->coords;
    gGfxViewCoord.flg = 0;
    coord             = &coords[index];
    Gp_UpdateCoord(&gGfxViewCoord);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, &local);
    out->x     = local.t[0];
    out->z     = local.t[2];
    coord->flg = 0;
}
