#include "common.h"
#include <psyq/libgte.h>

#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

#include "actors/actors_shared_80132614.h"
#include "actors/actor_215100.h"

/// Script opcode: yaws the actor's root coordinate to `placement->rot.vy`,
/// caching the yaw in the work block, and moves it to `placement->pos`.
s32 func_actor_215100_8014CDB0(Task* task, s32 arg1, ActorsShared80132614Placement* placement)
{
    GsCOORDINATE2*   coord;
    Actor215100Work* work;
    u16              yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor215100Work*)task->work;
    yaw       = placement->rot.vy;
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}
