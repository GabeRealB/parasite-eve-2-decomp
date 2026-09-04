#include "common.h"

#include "rooms/room_common.h"

/// Copies `placement` onto the task's `TmdObject` coordinate frame: the
/// three longs become the translation and the Euler angles are copied onto
/// the coordinate's rotation, then `RotMatrixZYX` rebuilds the matrix and
/// the coordinate is marked dirty.
s32 Room_Util18(Task* task, s32 arg1, RoomPlacement* placement, s32 arg3)
{
    RoomCoord* coord;

    coord             = (RoomCoord*)((TmdObject*)task->extra)->field_8;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->rot.vx     = placement->rot.vx;
    coord->rot.vy     = placement->rot.vy;
    coord->rot.vz     = placement->rot.vz;
    RotMatrixZYX(&coord->rot, &coord->coord);
    coord->flg = 0;
    return 0;
}
