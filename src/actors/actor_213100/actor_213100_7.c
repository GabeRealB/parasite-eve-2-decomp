#include "common.h"

#include "actors/actors_shared_8013231c.h"

#include "main/tmd.h"

/// Message-0x7D4 handler: places the actor at the message's arguments -
/// the translation goes straight into the root coordinate's local matrix, the
/// Euler angles into the coordinate's `rot` slot, from which the rotation is
/// rebuilt. Clearing `flg` has the world matrix recomputed. Returns 0.
s32 func_actor_213100_8014A390(Task* task, s32 arg1, ActorsShared8013231cArgs* args)
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
