#include "common.h"

#include <psyq/libgte.h>

#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_160600.h"

/// Script opcode "walk to": aims the actor's root coordinate at `target` by
/// taking the yaw of the horizontal offset from the coordinate's own
/// translation, caches that yaw in the work block and rebuilds the local
/// matrix from it, then records the remaining distance in twelfths as the
/// `travel` the step body's walk counts down.
s32 func_actor_160600_801326AC(Task* task, s32 arg1, Actor160600WalkTarget* target)
{
    GsCOORDINATE2*   coord;
    Actor160600Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor160600Work*)task->work;
    dx        = target->pos.vx - coord->coord.t[0];
    dz        = target->pos.vz - coord->coord.t[2];
    yaw       = ratan2(dx, dz);
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->travel = SquareRoot0(dx * dx + dz * dz) / 12;
    return 0;
}
