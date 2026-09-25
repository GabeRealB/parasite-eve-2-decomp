#include "common.h"

#include "actors/actor_535700.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// Visibility opcode of the second enemy: sets `TmdObject::flags` on its own
/// model and on the sub-model task's in `field_4B8` at once. `flags` bit 0
/// shows them (0) rather than hiding them (0x80), and bit 1 ORs 4 in.
s32 func_actor_535700_80133250(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = (TmdObject*)task->extra;
    other = (TmdObject*)((Actor535700SpawnWork*)task->work)->field_4B8->extra;

    if (flags & 1) {
        self->flags  = 0;
        other->flags = 0;
    } else {
        self->flags  = 0x80;
        other->flags = 0x80;
    }

    if (flags & 2) {
        self->flags  |= 4;
        other->flags |= 4;
    }
    return 0;
}

/// Placement opcode of the second enemy: yaws its root coordinate to
/// `placement->rot.vy`, caching that yaw in `Actor535700SpawnWork::yaw`, then
/// drops the placement translation into the matrix and marks it dirty.
s32 func_actor_535700_801332B4(Task* task, s32 arg1, Actor535700Placement* placement)
{
    GsCOORDINATE2*        coord;
    Actor535700SpawnWork* work;
    u16                   yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor535700SpawnWork*)task->work;
    yaw       = placement->rot.vy;
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}
