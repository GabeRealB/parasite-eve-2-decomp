#include "common.h"

#include "actors/actors_shared_801326ac.h"

#include "main/gfx.h"
#include "main/tmd.h"

/// Applies the "walk to" placement opcode: aims the actor's root coordinate at
/// `target` by taking the yaw of the horizontal offset from the coordinate's
/// own translation, caches that yaw in the overlay's work block and rebuilds
/// the local matrix from it, then records the remaining distance in twelfths
/// for the walk that follows.
s32 ActorsShared801326ac(Task* task, s32 arg1, ActorsShared801326acTarget* target)
{
    GsCOORDINATE2*            coord;
    ActorsShared801326acWork* work;
    s32                       dx;
    s32                       dz;
    u16                       yaw;

    coord     = ((TmdObject*)task->extra)->field_8;
    work      = (ActorsShared801326acWork*)task->idMap;
    dx        = target->pos.vx - coord->coord.t[0];
    dz        = target->pos.vz - coord->coord.t[2];
    yaw       = ratan2(dx, dz);
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->travel = SquareRoot0(dx * dx + dz * dz) / 12;
    return 0;
}
