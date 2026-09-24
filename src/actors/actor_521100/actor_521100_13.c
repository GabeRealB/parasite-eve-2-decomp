#include "common.h"

#include <psyq/libgte.h>

#include "actors/actors_shared_80132074.h"
#include "main/task.h"
#include "main/tmd.h"

/// Message 0x7D4 handler in `D_actor_521100_8015F6FC`, placing the actor: builds the root coordinate's
/// matrix from the argument block's angles, stores its translation and clears
/// `flg` so the world matrix is recomputed.
s32 func_actor_521100_80135CAC(Task* task, s32 arg1, ActorsShared80132074Args* args)
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
