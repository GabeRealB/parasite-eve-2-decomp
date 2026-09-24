#include "common.h"

#include "actors/actor_356100.h"
#include "actors/actors_shared_80169f74.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// Places the model's root coordinate from `placement` (translation, then the
/// X, Y and Z rotations in turn) and stores the resulting heading, `ratan2`
/// of the rotation's Z axis, in the work block's `yaw`.
s32 func_actor_356100_80169F74(Task* task, s32 arg1, ActorShared80169f74Placement* placement)
{
    GsCOORDINATE2*   coord;
    s32              mx;
    s32              mz;
    Actor356100Work* work;

    work                                          = (Actor356100Work*)task->work;
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
