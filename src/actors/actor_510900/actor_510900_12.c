#include "common.h"

#include "actors/actors_shared_80132074.h"

#include "main/task.h"
#include "main/tmd.h"

/// Message 0x7D4 handler: places the actor. It builds the model's root coordinate
/// from the argument block's Euler angles and translation, and clears its
/// `flg` so the world matrix is recomputed from it.
s32 func_actor_510900_8013BE00(Task* task, s32 arg1, ActorsShared80132074Args* args)
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
