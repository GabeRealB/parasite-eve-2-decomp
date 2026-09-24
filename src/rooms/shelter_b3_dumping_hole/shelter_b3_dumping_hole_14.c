#include "common.h"

#include "main/gfx.h"
#include "rooms/room_common.h"

/// Places the task's model at `placement`: the position becomes the
/// coordinate's translation, the rotation is applied in Y, X, Z order, and the
/// coordinate is marked for recomputation.
void func_shelter_b3_dumping_hole_80181854(Task* task, s32 arg1, RoomPlacement* placement)
{
    GsCOORDINATE2* coord;
    MATRIX*        mtx;

    coord             = ((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    mtx               = &coord->coord;
    coord->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(mtx, placement->rot.vy, 1);
    Gfx_RotMatrixX(mtx, placement->rot.vx, 0);
    Gfx_RotMatrixZ(mtx, placement->rot.vz, 0);
    coord->flg = 0;
}
