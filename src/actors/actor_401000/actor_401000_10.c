#include "common.h"

#include "actors/actor_401000.h"
#include "actors/actors_shared_80169f74.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// Places the actor's model from `placement`: the translation goes into the
/// root coordinate, then the X, Y and Z rotations are applied in that order.
/// The heading of the rotated Z axis is stored as the work block's `yaw`.
/// Returns 1.
s32 func_actor_401000_8013D814(Task* task, s32 arg1, ActorShared80169f74Placement* placement)
{
    GsCOORDINATE2*   coord;
    s32              mx;
    s32              mz;
    Actor401000Work* work;

    work                                          = (Actor401000Work*)task->work;
    ((TmdObject*)task->extra)->coords->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->coords->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->coords->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords->coord, placement->rot.vz, 0);
    ((TmdObject*)task->extra)->coords->flg = 0;
    coord                                  = ((TmdObject*)task->extra)->coords;
    mx                                     = coord->coord.m[2][0];
    mz                                     = coord->coord.m[2][2];
    work->yaw                              = ratan2(-mx, mz);
    return 1;
}
