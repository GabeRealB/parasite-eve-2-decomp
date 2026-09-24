#include "common.h"

#include "rooms/room_common.h"
#include "rooms/mine_forked_tunnel.h"

/// Puts the task's model at `placement`: the position becomes the translation
/// of the `TmdObject`'s coordinate frame and the angles its rotation, rebuilt
/// with `RotMatrixZYX` and marked dirty. Shaped as a message handler; the room
/// calls it directly with id 0x7D4 in `arg1`, which it does not read.
s32 func_mine_forked_tunnel_8017DC8C(Task* task, s32 arg1, RoomPlacement* placement, s32 arg3)
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
