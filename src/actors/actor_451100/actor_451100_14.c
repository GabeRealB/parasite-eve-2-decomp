#include "common.h"

#include "actors/actor_451100.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// Message 0x7D4 handler of `D_actor_451100_8013F704`, the placement opcode:
/// yaws the task's root coordinate to `placement->rot.vy`, caching that yaw in
/// the published work block, then drops the placement translation into the
/// matrix and marks it dirty.
s32 func_actor_451100_80132610(Task* task, s32 arg1, Actor451100Placement* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                        = ((TmdObject*)task->extra)->coords;
    D_actor_451100_8014E744->yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}
