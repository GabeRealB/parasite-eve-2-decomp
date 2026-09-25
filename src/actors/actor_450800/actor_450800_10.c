#include "common.h"

#include "main/gfx.h"
#include "main/tmd.h"

#include "actors/actor_450800.h"

/// Message handler 0x7D4 of `D_actor_450800_8014AC58`, the placement opcode:
/// yaws the actor's root coordinate to `placement->rot.vy`, caching that yaw in
/// `Actor450800Work::yaw`, then drops the placement translation into the matrix
/// and marks it dirty.
s32 func_actor_450800_80132C68(Task* task, s32 arg1, Actor450800Placement* placement)
{
    GsCOORDINATE2*   coord;
    Actor450800Work* work;
    u16              yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor450800Work*)task->work;
    yaw       = placement->rot.vy;
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}
