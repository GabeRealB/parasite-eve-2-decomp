#include "common.h"

#include "actors/actor_135600.h"

#include "main/task.h"
#include "main/tmd.h"

/// Drops the translation straight into the root part's local matrix, stores
/// the Euler angles in the coordinate's own `rot` slot and rebuilds the
/// rotation from them; clearing `flg` makes the world matrix be recomputed.
s32 func_actor_135600_801331C4(Task* task, s32 msgId, Actor135600PlaceArgs* args, s32 arg3)
{
    Actor135600Coord* coord;

    coord             = (Actor135600Coord*)((TmdObject*)task->extra)->coords;
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
