#include "common.h"

#include "actors/actor_210600.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// Message 0x7D4 handler, listed in `D_actor_210600_8015A4CC`: places the
/// model root at `placement`. The three longs become the coordinate's
/// translation, the X, Y and Z angles are then applied in that order through
/// `Gfx_RotMatrixX` / `Y` / `Z`, and the coordinate is marked dirty. `msgId`
/// is unused; the handler always reports the message handled.
s32 func_actor_210600_8014B6A0(Task* task, s32 msgId, Actor210600Placement* placement)
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
