#include "common.h"

#include "actors/actor_451100.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// Message 0x7D5 handler of `D_actor_451100_8014E6B4`: bit 0 of `flags`
/// clears `TmdObject::flags` on both the actor's model and its `pairTask`'s,
/// showing them, and its absence sets 0x80, hiding them; bit 1 additionally
/// ORs in 0x4.
s32 func_actor_451100_80132F04(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = (TmdObject*)task->extra;
    other = (TmdObject*)((Actor451100Work*)task->work)->pairTask->extra;

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

/// Message 0x7D4 handler of `D_actor_451100_8014E6B4`, the placement opcode:
/// yaws the actor's root coordinate to `placement->rot.vy`, caching that yaw
/// in the work block, then drops the placement translation into the matrix
/// and marks it dirty.
s32 func_actor_451100_80132F68(Task* task, s32 arg1, Actor451100Placement* placement)
{
    GsCOORDINATE2*   coord;
    Actor451100Work* work;
    u16              yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor451100Work*)task->work;
    yaw       = placement->rot.vy;
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}
