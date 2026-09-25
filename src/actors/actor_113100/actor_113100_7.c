#include "common.h"

#include <psyq/libgte.h>

#include "actors/actor_113100.h"
#include "actors/actors_shared_8013231c.h"

#include "main/task.h"
#include "main/tmd.h"

/// The 0x7D4 entry of `D_actor_113100_80144338`: places the actor at `args`.
/// The translation goes straight into the root part's local matrix, the Euler
/// angles into the coordinate's `rot` slot, from which `RotMatrix` rebuilds
/// the rotation; clearing `flg` makes the world matrix be recomputed.
s32 func_actor_113100_8013333C(Task* task, s32 msgId, Actor113100Placement* args)
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
