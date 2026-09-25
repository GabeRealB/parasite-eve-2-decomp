#include "common.h"

#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_150400.h"

/// Script opcode: set the visibility of the actor's model and of its sub-model
/// (the model of the task in `field_4B8`) together. `flags` bit 0 shows both
/// (`TmdObject::flags` = 0) and its absence hides them (0x80); bit 1
/// additionally sets bit 0x4 on both.
s32 func_actor_150400_80132710(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = (TmdObject*)task->extra;
    other = (TmdObject*)((Actor150400Work*)task->work)->field_4B8->extra;

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

/// Script opcode: place the actor. Yaws its root coordinate to
/// `placement->rot.vy`, caching that yaw in `yaw`, then drops the placement
/// translation into the matrix and marks it for recomputation.
s32 func_actor_150400_80132774(Task* task, s32 arg1, Actor150400Placement* placement)
{
    GsCOORDINATE2*   coord;
    Actor150400Work* work;
    u16              yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor150400Work*)task->work;
    yaw       = placement->rot.vy;
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}
