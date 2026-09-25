#include "common.h"
#include "actors/actors_shared_80132b88.h"
#include "actors/actors_shared_80133580.h"
#include "main/gfx.h"
#include "main/tmd.h"
#include "actors/actors_shared_80132514.h"
#include "actors/actors_shared_8014c874.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"
#include "main/task.h"

s32 ActorsShared80132b88(ActorShared80132b88* arg0, s32 arg1, ActorShared80132b88Cmd* arg2)
{
    arg0->field_1C->field_4EE = arg2->field_2;
    return 0;
}

/// Applies the "walk to" placement opcode: aims the actor's root coordinate at
/// `target` by taking the yaw of the horizontal offset from the coordinate's
/// own translation, caches that yaw in the overlay's work block and rebuilds
/// the local matrix from it, then records the remaining distance, scaled by
/// 30, for the walk that follows.
s32 ActorsShared80133580(Task* task, s32 arg1, ActorsShared80133580Target* target)
{
    GsCOORDINATE2*            coord;
    ActorsShared80133580Work* work;
    s32                       dx;
    s32                       dz;
    u16                       yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (ActorsShared80133580Work*)task->work;
    dx        = target->pos.vx - coord->coord.t[0];
    dz        = target->pos.vz - coord->coord.t[2];
    yaw       = ratan2(dx, dz);
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->travel = SquareRoot0(dx * dx + dz * dz) / 30;
    return 0;
}

void ActorsShared80132c6c(Task* task)
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
