#include "common.h"

#include "actors/actor_317000.h"

#include "main/task.h"
#include "main/tmd.h"

/// Message 0x7D4 handler of `D_actor_317000_8016CF50`: writes the payload's
/// position into the root coordinate's translation and its Euler angles into
/// `Actor317000Coord::rot`, rebuilds the rotation from them with `RotMatrix`
/// and clears `flg` so the world matrix is recomputed. Returns 0.
s32 func_actor_317000_80162B48(Task* task, s32 arg1, Actor317000Placement* args)
{
    Actor317000Coord* coord;

    coord             = (Actor317000Coord*)((TmdObject*)task->extra)->coords;
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
