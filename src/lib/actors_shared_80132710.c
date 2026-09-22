#include "common.h"
#include "actors/actors_shared_80132710.h"
#include "main/tmd.h"
#include "actors/actors_shared_80132774.h"
#include "main/gfx.h"

/// `flags` bit 0 hides both models (`TmdObject::flags` = 0) and its absence
/// restores the default 0x80; bit 1 additionally ORs in 0x4, the same bit
/// `Tmd_Create` sets for its own `flags & 1`.
s32 ActorsShared80132710(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = (TmdObject*)task->extra;
    other = (TmdObject*)((ActorsShared80132710Work*)task->work)->field_4B8->extra;

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

/// Applies the placement opcode: yaws the actor's root coordinate to
/// `placement->rot.vy`, caching that yaw in the overlay's work block, then
/// drops the placement translation into the matrix and marks it dirty.
s32 ActorsShared80132774(Task* task, s32 arg1, ActorsShared80132774Placement* placement)
{
    GsCOORDINATE2*            coord;
    ActorsShared80132774Work* work;
    u16                       yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (ActorsShared80132774Work*)task->work;
    yaw       = placement->rot.vy;
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}
