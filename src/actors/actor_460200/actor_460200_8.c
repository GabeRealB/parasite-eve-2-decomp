#include "common.h"

#include "actors/actor_460200.h"
#include "main/gfx.h"
#include "main/tmd.h"

/// Script opcode: set the work block's `field_4EE`, the head-turn flag, to the
/// payload.
s32 func_actor_460200_80133568(Task* task, s32 arg1, Actor460200LookArgs* args)
{
    ((Actor460200PairWork*)task->work)->field_4EE = args->look;
    return 0;
}

/// Script opcode "walk to": aims the actor's root coordinate at `target` by
/// taking the yaw of the horizontal offset from the coordinate's own
/// translation, caches that yaw in the work block and rebuilds the local
/// matrix from it, then records the distance, in steps of 30, for the walk
/// that follows.
s32 func_actor_460200_80133580(Task* task, s32 arg1, Actor460200WalkTarget* target)
{
    GsCOORDINATE2*       coord;
    Actor460200PairWork* work;
    s32                  dx;
    s32                  dz;
    u16                  yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor460200PairWork*)task->work;
    dx        = target->pos.vx - coord->coord.t[0];
    dz        = target->pos.vz - coord->coord.t[2];
    yaw       = ratan2(dx, dz);
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->travel = SquareRoot0(dx * dx + dz * dz) / 30;
    return 0;
}

/// Per-frame task of this actor's sub-model, with the sub-model's own
/// `TmdObject` in `Task::extra` and the actor as `Task::parent`. The first
/// frame lights the sub-model with the matrix pair at the front of the
/// parent's work block and hangs its coordinate off the parent model's eighth
/// coordinate; every frame marks the coordinate dirty.
void func_actor_460200_8013364C(Task* task)
{
    Task*          parent = task->parent;
    TmdObject*     obj    = task->extra;
    GsCOORDINATE2* coord  = obj->coords;
    GsCOORDINATE2* sub    = &((TmdObject*)parent->extra)->coords[7];
    MATRIX*        work   = (MATRIX*)parent->work;

    switch (task->state) {
        case 0:
            coord->flg    = 0;
            obj->lightMtx = work;
            obj->colorMtx = work + 1;
            coord->sub    = sub;
            task->state++;
            break;
        case 1:
            coord->flg = 0;
            break;
    }
}
