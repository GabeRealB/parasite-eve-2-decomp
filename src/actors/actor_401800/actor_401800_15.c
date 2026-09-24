#include "common.h"

#include "actors/actor_401800.h"
#include "actors/actors_shared_80169f74.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// Places the model from `placement`: copies the translation into the root
/// coordinate, applies the X, Y and Z rotations in that order (re-fetching the
/// coordinate for every field), then stores the resulting heading - `ratan2`
/// of the rotation matrix's Z axis - in the work block's `field_16`. Always
/// returns 1.
s32 func_actor_401800_8013DE3C(Task* task, s32 arg1, ActorShared80169f74Placement* placement)
{
    GsCOORDINATE2*   coord;
    s32              mx;
    s32              mz;
    Actor401800Work* work;

    work                                          = (Actor401800Work*)task->work;
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
    work->field_16                         = ratan2(-mx, mz);
    return 1;
}
