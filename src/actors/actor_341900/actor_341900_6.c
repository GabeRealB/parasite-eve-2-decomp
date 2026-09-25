#include "common.h"

#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_341900.h"

/// Message 0x7D4 handler of both of the overlay's message tables: copies the
/// placement onto the model's root coordinate, the three longs as its
/// translation and the three angles as its rotation (Y, then X, then Z), and
/// marks the coordinate dirty.
void func_actor_341900_801632A0(Task* task, s32 arg1, Actor341900MsgPos* placement)
{
    GsCOORDINATE2* coord;
    MATRIX*        mtx;

    coord             = ((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    mtx               = &coord->coord;
    coord->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(mtx, placement->rot.vy, 1);
    Gfx_RotMatrixX(mtx, placement->rot.vx, 0);
    Gfx_RotMatrixZ(mtx, placement->rot.vz, 0);
    coord->flg = 0;
}
