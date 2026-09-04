#include "common.h"

#include "main/gfx.h"
#include "rooms/room_common.h"

/// Copies `placement` onto the task's `TmdObject` coordinate frame: the
/// three longs become the translation, then yaw / pitch / roll are applied
/// with `Gfx_RotMatrixY` / `X` / `Z` and the coordinate is marked dirty.
void Room_Util08(Task* task, s32 arg1, RoomPlacement* placement)
{
    GsCOORDINATE2* coord;
    MATRIX*        mtx;

    coord             = ((TmdObject*)task->extra)->field_8;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    mtx               = &coord->coord;
    coord->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(mtx, placement->rot.vy, 1);
    Gfx_RotMatrixX(mtx, placement->rot.vx, 0);
    Gfx_RotMatrixZ(mtx, placement->rot.vz, 0);
    coord->flg = 0;
}
