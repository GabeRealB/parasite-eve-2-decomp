#include "common.h"

#include <psyq/libgte.h>

#include "actors/actor_120400.h"

#include "main/tmd.h"

/// Message 0x7D4 handler of the parent: places the root part at the message's
/// position and Euler angles, rebuilding the rotation from them and clearing
/// `flg` so the world matrix is recomputed. Returns 0.
s32 func_actor_120400_80132BBC(Task* task, s32 arg1, Actor120400Placement* args)
{
    Actor120400Coord* coord;

    coord             = (Actor120400Coord*)((TmdObject*)task->extra)->coords;
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
