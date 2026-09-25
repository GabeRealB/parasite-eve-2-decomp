#include "common.h"

#include "main/gfx.h"
#include "main/tmd.h"

#include "actors/actor_450800.h"

/// Message handler 0x7D5 of `D_actor_450800_801539AC`: sets the visibility
/// flags on the enemy's own model and on the sub-model task in `field_4B8` at
/// once. `flags` bit 0 hides both (`TmdObject::flags` = 0) and its absence
/// restores the default 0x80; bit 1 additionally ORs in 0x4.
s32 func_actor_450800_80133594(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = (TmdObject*)task->extra;
    other = (TmdObject*)((Actor450800SpawnWork*)task->work)->field_4B8->extra;

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

/// Message handler 0x7D4 of `D_actor_450800_801539AC`, the enemy's placement
/// opcode: yaws its root coordinate to `placement->rot.vy`, caching that yaw
/// in `Actor450800SpawnWork::yaw`, then drops the placement translation into
/// the matrix and marks it dirty.
s32 func_actor_450800_801335F8(Task* task, s32 arg1, Actor450800Placement* placement)
{
    GsCOORDINATE2*        coord;
    Actor450800SpawnWork* work;
    u16                   yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor450800SpawnWork*)task->work;
    yaw       = placement->rot.vy;
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}
