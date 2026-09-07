#include "common.h"

#include "actors/actors_shared_80169f74.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// Seeds the task's `TmdObject` coordinate frame from `placement` the same way
/// `ActorsShared80135990` does (X then Y then Z, re-fetching the coordinate
/// for every field), then stores the resulting heading -- `ratan2` of the
/// rotation matrix's Z-axis — in the work block's `yaw`.
s32 ActorsShared80169f74(Task* task, s32 arg1, ActorShared80169f74Placement* placement)
{
    GsCOORDINATE2*            coord;
    s32                       mx;
    s32                       mz;
    ActorsShared80169f74Work* work;

    work                                           = (ActorsShared80169f74Work*)task->idMap;
    ((TmdObject*)task->extra)->field_8->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->field_8->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->field_8->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vz, 0);
    ((TmdObject*)task->extra)->field_8->flg = 0;
    coord                                   = ((TmdObject*)task->extra)->field_8;
    mx                                      = coord->coord.m[2][0];
    mz                                      = coord->coord.m[2][2];
    work->yaw                               = ratan2(-mx, mz);
    return 1;
}
