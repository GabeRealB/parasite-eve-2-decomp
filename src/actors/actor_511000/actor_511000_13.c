#include "common.h"

#include "actors/actor_511000.h"
#include "actors/actors_shared_801334c4.h"

#include "main/task.h"
#include "main/tmd.h"

/// Placement message handler that also shows the model: writes the payload's
/// translation and Euler angles into the root coordinate, rebuilds its
/// rotation, clears `flg` so the world matrix is recomputed and clears the
/// model's hidden bit 0x80.
s32 func_actor_511000_801334C4(Task* task, s32 arg1, ActorsShared801334c4Args* args, s32 arg3)
{
    Actor511000Coord* coord;
    TmdObject*        extra;

    extra             = (TmdObject*)task->extra;
    coord             = (Actor511000Coord*)extra->coords;
    coord->coord.t[0] = args->pos.vx;
    coord->coord.t[1] = args->pos.vy;
    coord->coord.t[2] = args->pos.vz;
    coord->rot.vx     = args->rot.vx;
    coord->rot.vy     = args->rot.vy;
    coord->rot.vz     = args->rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg    = 0;
    extra->flags &= 0xFF7F;
    return 0;
}
