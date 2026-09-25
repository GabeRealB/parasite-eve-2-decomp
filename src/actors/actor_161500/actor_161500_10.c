#include "common.h"

#include "actors/actor_161500.h"
#include "main/gfx.h"
#include "main/tmd.h"

/// Script opcode "place at": yaws the actor's root coordinate to
/// `placement->rot.vy`, caching that yaw in the work block, then drops the
/// placement translation into the matrix and marks it dirty.
s32 func_actor_161500_80132B10(Task* task, s32 arg1, Actor161500Placement* placement)
{
    GsCOORDINATE2*   coord;
    Actor161500Work* work;
    u16              yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor161500Work*)task->work;
    yaw       = placement->rot.vy;
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}
