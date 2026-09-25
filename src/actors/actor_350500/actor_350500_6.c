#include "common.h"

#include <psyq/libgte.h>

#include "actors/actor_350500.h"

#include "main/task.h"
#include "main/tmd.h"

/// Message-0x7D4 handler: places the root part at `args`. The translation
/// goes straight into the local matrix, the Euler angles into the
/// coordinate's `rot` slot, from which `RotMatrix` rebuilds the rotation;
/// clearing `flg` makes the world matrix be recomputed. Returns 0.
s32 func_actor_350500_80162960(Task* task, s32 msgId, Actor350500Placement* args)
{
    Actor350500Coord* coord;

    coord             = (Actor350500Coord*)((TmdObject*)task->extra)->coords;
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
