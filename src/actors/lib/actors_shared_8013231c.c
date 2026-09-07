#include "common.h"

#include "actors/actors_shared_8013231c.h"

#include "main/tmd.h"

s32 ActorsShared8013231c(Task* task, s32 arg1, ActorsShared8013231cArgs* args)
{
    ActorsShared8013231cCoord* coord;

    coord             = (ActorsShared8013231cCoord*)((TmdObject*)task->extra)->field_8;
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
