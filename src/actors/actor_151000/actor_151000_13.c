#include "common.h"

#include "actors/actor_151000.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// Placement opcode: yaws the model's root coordinate to `placement->yaw`,
/// caching that yaw in the work block, then drops the placement translation
/// into the matrix and marks it dirty.
s32 func_actor_151000_80132810(Task* task, s32 arg1, Actor151000Placement* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                        = ((TmdObject*)task->extra)->coords;
    D_actor_151000_8013D37C->yaw = yaw = placement->yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}
