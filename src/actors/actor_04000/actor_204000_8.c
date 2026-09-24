#include "common.h"

#include "actors/actors_shared_80135990.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// Handler for message 0x7D4: places the actor's model from `placement`. The
/// three longs become the coordinate's translation, then the X, Y and Z angles
/// are applied in that order and the coordinate is marked dirty. Always
/// answers 1.
s32 Actor04000_Fn06634(Task* task, s32 arg1, ActorShared80135990Placement* placement)
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
