#include "common.h"

#include "actors/actor_146300.h"
#include "actors/actors_shared_8013411c.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

/// Placement message handler: turns the model root to `placement`'s yaw
/// (recorded in the work block's `yaw`), moves it to `placement`'s position and
/// marks the coordinate for recomputation. Only the Y rotation is applied.
s32 func_actor_146300_80132A98(Task* task, s32 arg1, ActorShared8013411cPlacement* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                        = ((TmdObject*)task->extra)->coords;
    D_actor_146300_80142828->yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}
