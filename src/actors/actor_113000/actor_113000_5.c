#include "common.h"

#include <psyq/libgte.h>

#include "actors/actors_shared_8013231c.h"

#include "main/task.h"
#include "main/tmd.h"

/// Placement handler: writes the payload's position into the root
/// coordinate's translation and its Euler angles into the coordinate's `rot`
/// slot, rebuilds the rotation from them with `RotMatrix` and clears `flg` so
/// the world matrix is recomputed. Returns 0.
s32 func_actor_113000_8013231C(Task* task, s32 arg1, ActorsShared8013231cArgs* args)
{
    ActorsShared8013231cCoord* coord;

    coord             = (ActorsShared8013231cCoord*)((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = args->pos.vx;
    coord->coord.t[1] = args->pos.vy;
    coord->coord.t[2] = args->pos.vz;
    coord->rot.vx     = args->rot.vx;
    coord->rot.vy     = args->rot.vy;
    coord->rot.vz     = args->rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
    return 0;
}
