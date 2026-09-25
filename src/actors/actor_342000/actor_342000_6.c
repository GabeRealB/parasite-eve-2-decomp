#include "common.h"

#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_342000.h"

/// Message 0x7D4 handler of the table at `D_actor_342000_801648A8`: copies
/// the payload onto the model's root coordinate, the three longs as its
/// translation and the three angles as its rotation (Y, then X, then Z), and
/// marks the coordinate dirty.
void func_actor_342000_80164034(Task* task, s32 arg1, Actor342000Move* arg2)
{
    GsCOORDINATE2* coord;
    MATRIX*        mtx;

    coord             = ((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = arg2->field_0;
    coord->coord.t[1] = arg2->field_4;
    mtx               = &coord->coord;
    coord->coord.t[2] = arg2->field_8;
    Gfx_RotMatrixY(mtx, arg2->field_12, 1);
    Gfx_RotMatrixX(mtx, arg2->field_10, 0);
    Gfx_RotMatrixZ(mtx, arg2->field_14, 0);
    coord->flg = 0;
}
