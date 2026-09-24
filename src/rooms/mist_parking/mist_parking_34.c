#include "common.h"

#include "rooms/room_common.h"
#include "rooms/mist_parking.h"

/// Places the task's model at `placement`: its position becomes the
/// coordinate frame's translation, its angles the frame's rotation, from
/// which `RotMatrixZYX` rebuilds the matrix; clearing `flg` makes the frame
/// be recomputed.
s32 func_mist_parking_80183AC4(Task* task, s32 arg1, RoomPlacement* placement, s32 arg3)
{
    RoomCoord* coord;

    coord             = (RoomCoord*)((TmdObject*)task->extra)->coords;
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
