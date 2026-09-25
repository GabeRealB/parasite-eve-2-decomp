#include "common.h"

#include "actors/actor_535700.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// Placement opcode of the first enemy: yaws the model's root coordinate to
/// `placement->rot.vy`, caching that yaw in the work block's `field_4AE`, then
/// drops the placement translation into the matrix and marks it dirty.
s32 func_actor_535700_80132894(Task* task, s32 arg1, Actor535700Placement* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                              = ((TmdObject*)task->extra)->coords;
    D_actor_535700_80146844->field_4AE = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}
