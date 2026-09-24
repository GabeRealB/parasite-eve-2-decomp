#include "common.h"

#include "actors/actors_shared_80133c6c.h"
#include "gameplay/3CD8.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// Message 0x7D4 handler: places the task's model in the world. The model's
/// coordinate is parented to the view coordinate, takes `placement`'s
/// position as its translation and its rotation applied Y, then X, then Z.
/// `arg1` is the message id.
void func_actor_120300_80133C6C(Task* task, s32 arg1, ActorShared80133c6cPlacement* placement)
{
    GsCOORDINATE2* coord;
    MATRIX*        mtx;

    coord             = ((TmdObject*)task->extra)->coords;
    coord->sub        = &gGfxViewCoord;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    mtx               = &coord->coord;
    coord->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(mtx, placement->rot.vy, 1);
    Gfx_RotMatrixX(mtx, placement->rot.vx, 0);
    Gfx_RotMatrixZ(mtx, placement->rot.vz, 0);
    coord->flg = 0;
}
