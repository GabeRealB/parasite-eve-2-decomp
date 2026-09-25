#include "common.h"

#include "main/gfx.h"
#include "main/tmd.h"

#include "actors/actor_450800.h"

/// Message handler 0x7DD of `D_actor_450800_801539AC`, the enemy's "walk to"
/// opcode: turns its root coordinate to face `target`, caching the yaw in
/// `Actor450800SpawnWork::yaw`, and leaves the horizontal distance to it, in
/// twelfths, in `travel` for the walk state to count down.
s32 func_actor_450800_80133678(Task* task, s32 arg1, VECTOR* target)
{
    GsCOORDINATE2*        coord;
    Actor450800SpawnWork* work;
    s32                   dx;
    s32                   dz;
    u16                   yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor450800SpawnWork*)task->work;
    dx        = target->vx - coord->coord.t[0];
    dz        = target->vz - coord->coord.t[2];
    yaw       = ratan2(dx, dz);
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->travel = SquareRoot0(dx * dx + dz * dz) / 12;
    return 0;
}
