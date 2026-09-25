#include "common.h"

#include "actors/actors_shared_80132074.h"

#include "main/task.h"
#include "main/tmd.h"

/// Placement message handler: builds the root coordinate's matrix from the
/// payload's Euler angles, drops its translation in and clears `flg` so the
/// world matrix is recomputed.
s32 func_actor_511000_80133E48(Task* task, s32 arg1, ActorsShared80132074Args* args)
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
