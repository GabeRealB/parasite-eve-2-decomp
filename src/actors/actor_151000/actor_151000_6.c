#include "common.h"

#include <psyq/libgte.h>

#include "actors/actor_151000.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// "Walk to" opcode: records `mode` in `D_actor_151000_8013D384`, turns the
/// model to face `target` (away from it in mode 1) caching the yaw in the work
/// block, and leaves in `travel` the planar distance divided by the walk's
/// frame count: 0x3C in mode 0, 0xF in mode 1 and 0x19 in mode 2.
s32 func_actor_151000_801328DC(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GsCOORDINATE2*   coord;
    Actor151000Work* work;
    s32              dx;
    s32              dz;
    s32              steps;
    s32              dist;
    s32              angle;

    coord                   = ((TmdObject*)task->extra)->coords;
    work                    = (Actor151000Work*)task->work;
    D_actor_151000_8013D384 = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->yaw               = angle;
    if (D_actor_151000_8013D384 == 1) {
        work->yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->yaw, 1);
    dist = SquareRoot0(dx * dx + dz * dz);
    switch (D_actor_151000_8013D384) {
        case 0:
            steps = 0x3C;
            break;
        case 1:
            steps = 0xF;
            break;
        case 2:
            steps = 0x19;
            break;
    }
    work->travel = dist / steps;
    return 0;
}
