#include "common.h"

#include "actors/actors_shared_80132074.h"

#include "main/tmd.h"

/// Message 0x7D4 handler: places the actor. Builds the root coordinate's
/// rotation from the message's Euler angles, writes its translation, and
/// clears `flg` so the world matrix is recomputed from them.
s32 func_actor_110700_80132074(Task* task, s32 arg1, ActorsShared80132074Args* args)
{
    TmdObject*     ext   = task->extra;
    GsCOORDINATE2* coord = ext->coords;

    RotMatrix(&args->rot, &coord->coord);
    coord->coord.t[0] = args->pos.vx;
    coord->coord.t[1] = args->pos.vy;
    coord->coord.t[2] = args->pos.vz;
    coord->flg        = 0;
    return 0;
}
