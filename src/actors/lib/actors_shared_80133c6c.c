#include "common.h"

#include "actors/actors_shared_80133c6c.h"
#include "gameplay/3CD8.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// `ActorsShared8013411c` with the coordinate re-parented to the world frame
/// first: `placement` becomes the task's `TmdObject` coordinate, with the three
/// longs as the translation and the three shorts applied as yaw / pitch / roll.
void ActorsShared80133c6c(Task* task, s32 arg1, ActorShared80133c6cPlacement* placement)
{
    GsCOORDINATE2* coord;
    MATRIX*        mtx;

    coord             = ((TmdObject*)task->extra)->field_8;
    coord->sub        = &Gfx_ViewCoord;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    mtx               = &coord->coord;
    coord->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(mtx, placement->rot.vy, 1);
    Gfx_RotMatrixX(mtx, placement->rot.vx, 0);
    Gfx_RotMatrixZ(mtx, placement->rot.vz, 0);
    coord->flg = 0;
}
