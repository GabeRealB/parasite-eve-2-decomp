#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "rooms/room_common.h"

extern RoomPlacement D_mine_forked_tunnel_80181BA4;

INCLUDE_ASM("rooms/nonmatchings/mine_forked_tunnel/mine_forked_tunnel_2", func_mine_forked_tunnel_8017DD08);

INCLUDE_ASM("rooms/nonmatchings/mine_forked_tunnel/mine_forked_tunnel_2", func_mine_forked_tunnel_8017DDE8);

/// Spawn state 0: adopt the parent task's model lighting - the light and colour
/// matrix pointers off the parent's `TmdObject` plus its coordinate as the
/// frame's parent link - then reparent onto that task, drop `field_C` bit 7 and
/// place the object at this room's `RoomPlacement`, rebuilding `coord` with
/// `RotMatrixZYX`.
void func_mine_forked_tunnel_8017DE54(Task* task)
{
    Task*          parent;
    TmdObject*     ext;
    TmdObject*     parentExt;
    GsCOORDINATE2* parentCoord;
    RoomCoord*     coord;
    RoomCoord*     dst;

    parent      = task->spawnArg2;
    ext         = task->extra;
    parentExt   = parent->extra;
    coord       = (RoomCoord*)ext->field_8;
    parentCoord = parentExt->field_8;

    coord->flg    = 0;
    coord->sub    = parentCoord;
    ext->field_1C = parentExt->field_1C;
    ext->field_20 = parentExt->field_20;
    ext->field_E  = -1;
    Task_Reparent(parent, task);
    ext->field_C = ext->field_C & 0xFF7F;

    dst             = (RoomCoord*)((TmdObject*)task->extra)->field_8;
    dst->coord.t[0] = D_mine_forked_tunnel_80181BA4.pos.vx;
    dst->coord.t[1] = D_mine_forked_tunnel_80181BA4.pos.vy;
    dst->coord.t[2] = D_mine_forked_tunnel_80181BA4.pos.vz;
    dst->rot.vx     = D_mine_forked_tunnel_80181BA4.rot.vx;
    dst->rot.vy     = D_mine_forked_tunnel_80181BA4.rot.vy;
    dst->rot.vz     = D_mine_forked_tunnel_80181BA4.rot.vz;
    RotMatrixZYX(&dst->rot, &dst->coord);
    dst->flg    = 0;
    task->state = task->state + 1;
}

INCLUDE_ASM("rooms/nonmatchings/mine_forked_tunnel/mine_forked_tunnel_2", func_mine_forked_tunnel_8017DF34);
