#include "common.h"

#include "actors/actors_shared_80135990.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// Seeds the task's `TmdObject` coordinate frame from `placement`: the three
/// longs become the translation, then pitch / yaw / roll are applied with
/// `Gfx_RotMatrixX` / `Y` / `Z` and the coordinate is marked dirty. Unlike
/// `ActorsShared8013411c` the rotations go X first and the coordinate is
/// re-fetched for every field, and the caller gets a constant 1 back.
s32 ActorsShared80135990(Task* task, s32 arg1, ActorShared80135990Placement* placement)
{
    ((TmdObject*)task->extra)->field_8->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->field_8->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->field_8->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vz, 0);
    ((TmdObject*)task->extra)->field_8->flg = 0;
    return 1;
}
