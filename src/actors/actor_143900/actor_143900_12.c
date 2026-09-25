#include "common.h"

#include "actors/actor_143900.h"
#include "main/gfx.h"
#include "main/tmd.h"

/// Seeds the task's `TmdObject` coordinate frame from `placement`: only the yaw
/// is used, remembered in the first variant's work block and applied with
/// `Gfx_RotMatrixY`, then the three longs become the coordinate's translation.
s32 func_actor_143900_801326FC(Task* task, s32 arg1, ActorShared8013411cPlacement* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                        = ((TmdObject*)task->extra)->coords;
    D_actor_143900_801496B8->yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}
