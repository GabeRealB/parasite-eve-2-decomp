#include "common.h"

#include "actors/actor_461800.h"
#include "actors/actors_shared_8013411c.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// Seeds the task's `TmdObject` coordinate frame from `placement`: only the yaw
/// is used, remembered in the work block and applied with `Gfx_RotMatrixY`,
/// then the three longs become the coordinate's translation.
s32 func_actor_461800_80133970(Task* task, s32 arg1, ActorShared8013411cPlacement* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                              = ((TmdObject*)task->extra)->field_8;
    D_actor_461800_801438A0->field_4AE = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Message handler: the message id selects how the second work block is
/// reseeded -- 0 arms the reset argument, 1 remembers the id in the byte the
/// seeding loop reads. Anything else does nothing.
s32 func_actor_461800_801339EC(Task* task, s32 arg1, Actor461800Msg* msg, s32 arg3)
{
    s32 id;

    id = msg->field_2;
    switch (id) {
        case 0:
            D_actor_461800_801438A0->field_4B4 = 0x14;
            break;
        case 1:
            D_actor_461800_801438A0->field_4BC = id;
            break;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_4", func_actor_461800_80133A3C);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_4", func_actor_461800_80133B98);
