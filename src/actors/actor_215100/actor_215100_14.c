#include "common.h"
#include <psyq/libgte.h>

#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

#include "actors/actors_shared_801326ac.h"
#include "actors/actor_215100.h"

/// Script opcode "walk to": turns the actor's root coordinate to face
/// `target` horizontally, caching the yaw, and stores the horizontal distance
/// in steps of 12 as `travel` for the step body to walk off.
s32 func_actor_215100_8014CE30(Task* task, s32 arg1, ActorsShared801326acTarget* target)
{
    GsCOORDINATE2*   coord;
    Actor215100Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor215100Work*)task->work;
    dx        = target->pos.vx - coord->coord.t[0];
    dz        = target->pos.vz - coord->coord.t[2];
    yaw       = ratan2(dx, dz);
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->travel = SquareRoot0(dx * dx + dz * dz) / 12;
    return 0;
}
