#include "common.h"

#include <psyq/libgte.h>

#include "actors/actor_361100.h"

#include "main/task.h"
#include "main/tmd.h"

/// Message 0x7D4 handler, listed in `D_actor_361100_80171BB8`: places the
/// model at once. Writes the payload's translation into the root coordinate,
/// keeps its Euler angles in the coordinate's `rot` slot and rebuilds the
/// rotation from them with `RotMatrix`, then clears `flg` so the world matrix
/// is recomputed.
s32 func_actor_361100_801635F4(Task* task, s32 arg1, Actor361100Placement* placement)
{
    Actor361100Coord* coord;

    coord             = (Actor361100Coord*)((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->rot.vx     = placement->rot.vx;
    coord->rot.vy     = placement->rot.vy;
    coord->rot.vz     = placement->rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
    return 0;
}
