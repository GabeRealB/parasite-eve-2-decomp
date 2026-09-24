#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "rooms/room_common.h"
#include "rooms/mine_forked_tunnel.h"

extern u8 D_801153F4;

/// State table of the enemy's pitch-animated child, indexed by `Task::state`.
extern const TaskFuncTable3 D_mine_forked_tunnel_8017D5D0;

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

/// Dispatches the enemy's pitch-animated child through its three-state table
/// (attach, pitch walk, `taskKill`), copied onto the stack first; nothing runs
/// while `D_801153F4` is non-zero.
void func_mine_forked_tunnel_8017DDE8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mine_forked_tunnel_8017D5D0;
    if (D_801153F4 == 0) {
        sp.funcs[task->state](task);
    }
}

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

/// Restores the room's layout lists from their template, then offsets the eight
/// `field_8` coordinates by (0, 0, -0xC8), or by (0, -0xBB8, -0xC8) when `arg0`
/// is non-zero. The callers pass game-flag nibble 0x75.
void func_mine_forked_tunnel_8017DF34(s32 arg0)
{
    MineForkedTunnelLayout* dst;
    MineForkedTunnelLayout* src;
    MineForkedTunnelVec     d;
    s32                     i;

    dst = &D_mine_forked_tunnel_80183D70;
    src = &D_mine_forked_tunnel_80181C5C;

    for (i = 0; i < 3; i++) {
        dst->field_4[i].x = src->field_4[i].x;
        dst->field_4[i].y = src->field_4[i].y;
        dst->field_4[i].z = src->field_4[i].z;
        dst->field_C[i]   = src->field_C[i];
    }

    for (i = 0; i < 8; i++) {
        dst->field_8[i].x = src->field_8[i].x;
        dst->field_8[i].y = src->field_8[i].y;
        dst->field_8[i].z = src->field_8[i].z;
    }

    if (arg0 == 0) {
        d.x = 0;
        d.y = 0;
        d.z = -0xC8;
    } else {
        d.y = -0xBB8;
        d.x = 0;
        d.z = -0xC8;
    }

    for (i = 0; i < 8; i++) {
        dst->field_8[i].x += d.x;
        dst->field_8[i].y += d.y;
        dst->field_8[i].z += d.z;
    }
}
