#include "common.h"

#include "actors/actor_535700.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// "Walk to" opcode of the second enemy: turns its root coordinate to face
/// `target`, caching the yaw in `Actor535700SpawnWork::yaw`, and leaves the
/// horizontal distance to it, in twelfths, in `travel` for the walk state to
/// count down.
s32 func_actor_535700_80133334(Task* task, s32 arg1, VECTOR* target)
{
    GsCOORDINATE2*        coord;
    Actor535700SpawnWork* work;
    s32                   dx;
    s32                   dz;
    u16                   yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor535700SpawnWork*)task->work;
    dx        = target->vx - coord->coord.t[0];
    dz        = target->vz - coord->coord.t[2];
    yaw       = ratan2(dx, dz);
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->travel = SquareRoot0(dx * dx + dz * dz) / 12;
    return 0;
}
