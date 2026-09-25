#include "common.h"

#include <psyq/libgte.h>

#include "actors/actor_451100.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// Message 0x7DD handler of `D_actor_451100_8014E6B4`, the "walk to" opcode:
/// turns the actor's root coordinate to face `target`, caching the yaw in the
/// work block, and leaves the horizontal distance to it, in seventeenths, in
/// `travel` for the step routine to count down.
s32 func_actor_451100_80132FE8(Task* task, s32 arg1, VECTOR* target)
{
    GsCOORDINATE2*   coord;
    Actor451100Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor451100Work*)task->work;
    dx        = target->vx - coord->coord.t[0];
    dz        = target->vz - coord->coord.t[2];
    yaw       = ratan2(dx, dz);
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->travel = SquareRoot0(dx * dx + dz * dz) / 17;
    return 0;
}
