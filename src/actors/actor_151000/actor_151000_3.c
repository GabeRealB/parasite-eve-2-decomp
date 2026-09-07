#include "common.h"

#include "actors/actor_151000.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000_3", func_actor_151000_801324FC);

/// Ticks animation slots 1..0x12 of the actor's animation context.
void func_actor_151000_801325C4(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&ActorsShared80131f9cWork->anim, i);
        i++;
    } while (i < 0x13);
}

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000_3", func_actor_151000_80132610);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000_3", func_actor_151000_801326AC);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000_3", func_actor_151000_80132738);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000_3", func_actor_151000_801327C8);

/// Seeds the task's `TmdObject` coordinate frame from `placement`: the yaw is
/// remembered in the work block and applied with `Gfx_RotMatrixY`, then the
/// three longs become the coordinate's translation.
s32 func_actor_151000_80132810(Task* task, s32 arg1, Actor151000Placement* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                         = ((TmdObject*)task->extra)->field_8;
    ActorsShared80131f9cWork->yaw = yaw = placement->yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000_3", func_actor_151000_8013288C);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000_3", func_actor_151000_801328DC);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000_3", func_actor_151000_80132A38);
