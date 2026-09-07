#include "common.h"

#include "actors/actors_shared_80132074.h"

#include "main/tmd.h"

s32 ActorsShared80132074(Task* task, s32 arg1, ActorsShared80132074Args* args)
{
    TmdObject*     ext   = task->extra;
    GsCOORDINATE2* coord = ext->field_8;

    RotMatrix(&args->rot, &coord->coord);
    coord->coord.t[0] = args->pos.vx;
    coord->coord.t[1] = args->pos.vy;
    coord->coord.t[2] = args->pos.vz;
    coord->flg        = 0;
    return 0;
}
