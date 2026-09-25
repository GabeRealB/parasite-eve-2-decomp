#include "common.h"

#include "actors/actor_460200.h"
#include "main/gfx.h"
#include "main/tmd.h"

s32 func_actor_460200_80133DC4(void)
{
    return 0;
}

/// Script opcode "walk to": aims the actor's root coordinate at `target` by
/// taking the yaw of the horizontal offset from the coordinate's own
/// translation, caches that yaw in the work block and rebuilds the local
/// matrix from it, then records the distance, in steps of 12, for the step
/// body to walk off.
s32 func_actor_460200_80133DCC(Task* task, s32 arg1, Actor460200WalkTarget* target)
{
    GsCOORDINATE2*   coord;
    Actor460200Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor460200Work*)task->work;
    dx        = target->pos.vx - coord->coord.t[0];
    dz        = target->pos.vz - coord->coord.t[2];
    yaw       = ratan2(dx, dz);
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->travel = SquareRoot0(dx * dx + dz * dz) / 12;
    return 0;
}
