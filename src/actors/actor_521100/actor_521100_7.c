#include "common.h"

#include "actors/actor_521100.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_7", func_actor_521100_80136A64);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_7", func_actor_521100_80136AE0);

s32 func_actor_521100_80136BE8(Task* task, s32 arg1, Actor521100Target* target)
{
    GsCOORDINATE2* coord;
    s32            dx;
    s32            dz;
    u16            yaw;

    coord = ((TmdObject*)task->extra)->field_8;
    dx    = target->pos.vx - coord->coord.t[0];
    dz    = target->pos.vz - coord->coord.t[2];
    yaw   = ratan2(dx, dz);
    D_actor_521100_8016A3D8->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    D_actor_521100_8016A3D8->travel = SquareRoot0(dx * dx + dz * dz) / 20;
    return 0;
}
