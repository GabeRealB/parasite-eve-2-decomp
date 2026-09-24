#include "common.h"

#include "actors/actors_shared_80135990.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// Seeds the task's `TmdObject` root coordinate from `placement`: the three
/// longs become the translation, then pitch / yaw / roll are applied with
/// `Gfx_RotMatrixX` / `Y` / `Z`, re-fetching the coordinate for every field,
/// and the coordinate is marked dirty. Always returns 1.
s32 func_actor_341700_8016CF48(Task* task, s32 arg1, ActorShared80135990Placement* placement)
{
    ((TmdObject*)task->extra)->coords->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->coords->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->coords->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords->coord, placement->rot.vz, 0);
    ((TmdObject*)task->extra)->coords->flg = 0;
    return 1;
}
