#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "rooms/room_common.h"
#include "rooms/mine_forked_tunnel.h"

/// `Task::msgTable` handler for message id 0x7D5: switches the draw and
/// buffer-alloc bits of the task's `TmdObject` extra. Modes 0 and 1 set and
/// clear bit 0x80 - hiding and showing the model - and leave bit 0x4 clear so
/// the model keeps its buffers, mode 1 reinstating them through
/// `Tmd_AllocBuffers` first. Modes 2 and 3 set 0x4 instead, skipping that
/// allocation; mode 2 also stores itself in the work block's lifetime counter,
/// `MineForkedTunnelWork::field_44`, which `func_mine_forked_tunnel_8017D724`
/// counts down before freeing the child. Any other mode touches nothing and
/// reports 1.
s32 func_mine_forked_tunnel_8017DD08(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject* ext;
    s32        ret;

    ext = (TmdObject*)task->extra;
    ret = 0;
    switch (mode) {
        case 0:
            ext->flags |= 0x80;
            ext->flags &= ~4;
            break;
        case 1:
            ext->flags &= ~0x80;
            Tmd_AllocBuffers(ext);
            ext->flags &= ~4;
            break;
        case 2:
            ext->flags                                   |= 0x80;
            ((MineForkedTunnelWork*)task->work)->field_44 = mode;
            ext->flags                                   |= 4;
            break;
        case 3:
            ext->flags &= ~0x80;
            ext->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

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
    coord       = (RoomCoord*)ext->coords;
    parentCoord = parentExt->coords;

    coord->flg    = 0;
    coord->sub    = parentCoord;
    ext->lightMtx = parentExt->lightMtx;
    ext->colorMtx = parentExt->colorMtx;
    ext->otOffset = -1;
    Task_Reparent(parent, task);
    ext->flags = ext->flags & 0xFF7F;

    dst             = (RoomCoord*)((TmdObject*)task->extra)->coords;
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
