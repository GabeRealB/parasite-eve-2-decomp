#include "common.h"

#include <psyq/libgte.h>

#include "actors/actor_141000.h"

#include "main/task.h"
#include "main/tmd.h"

/// Message-0x7D4 handler: places the model at once. Writes the payload's
/// translation into the root coordinate, keeps its Euler angles in the
/// coordinate's `rot` slot and rebuilds the rotation from them, then clears
/// `flg` so the world matrix is recomputed.
s32 func_actor_141000_80133E10(Task* task, s32 arg1, Actor141000Placement* args)
{
    Actor141000Coord* coord;

    coord             = (Actor141000Coord*)((TmdObject*)task->extra)->coords;
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
