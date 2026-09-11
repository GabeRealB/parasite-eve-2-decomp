#include "common.h"

#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actors_shared_80139dcc.h"

/// Refreshes the view coordinate and coordinate `index` of the actor's model,
/// then stores that coordinate's view-space X/Z translation to `out`.
void ActorsShared80139dcc(Task* task, s16 index, ActorsShared80139dccPos* out)
{
    MATRIX         local;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* coords;

    coords            = (GsCOORDINATE2*)((TmdObject*)task->extra)->field_8;
    Gfx_ViewCoord.flg = 0;
    coord             = &coords[index];
    Gp_UpdateCoord(&Gfx_ViewCoord);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &coord->workm, &local);
    out->x     = local.t[0];
    out->z     = local.t[2];
    coord->flg = 0;
}
