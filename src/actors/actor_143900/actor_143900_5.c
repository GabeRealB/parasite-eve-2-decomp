#include "common.h"

#include "actors/actor_143900.h"
#include "main/gfx.h"
#include "main/tmd.h"

/// Seeds the task's `TmdObject` coordinate frame from `placement`: only the yaw
/// is used, remembered in the work block and applied with `Gfx_RotMatrixY`,
/// then the three longs become the coordinate's translation.
s32 func_actor_143900_801326FC(Task* task, s32 arg1, ActorShared8013411cPlacement* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                         = ((TmdObject*)task->extra)->field_8;
    ActorsShared80131f9cWork->yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_5", func_actor_143900_80132778);

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_5", func_actor_143900_8013279C);

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_5", func_actor_143900_801328D4);

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_5", func_actor_143900_80132A9C);

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_5", func_actor_143900_80132DEC);

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_5", func_actor_143900_80132E48);
