#include "common.h"

#include "actors/actor_511000.h"
#include "actors/actors_shared_8013231c.h"

#include "main/task.h"
#include "main/tmd.h"

/// Placement message handler: writes the payload's translation into the root
/// coordinate, keeps its Euler angles in the coordinate's `rot` slot and
/// rebuilds the rotation from them, then clears `flg` so the world matrix is
/// recomputed.
s32 func_actor_511000_80132724(Task* task, s32 arg1, ActorsShared8013231cArgs* args)
{
    Actor511000Coord* coord;

    coord             = (Actor511000Coord*)((TmdObject*)task->extra)->coords;
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
