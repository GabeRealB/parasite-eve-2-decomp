#include "common.h"

#include "actors/actors_shared_80132614.h"

#include "main/gfx.h"
#include "main/tmd.h"

/// Applies the placement opcode: yaws the actor's root coordinate to
/// `placement->rot.vy`, caching that yaw in the overlay's work block, then
/// drops the placement translation into the matrix and marks it dirty.
s32 ActorsShared80132614(Task* task, s32 arg1, ActorsShared80132614Placement* placement)
{
    GsCOORDINATE2*            coord;
    ActorsShared80132614Work* work;
    u16                       yaw;

    coord     = ((TmdObject*)task->extra)->field_8;
    work      = (ActorsShared80132614Work*)task->idMap;
    yaw       = placement->rot.vy;
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}
