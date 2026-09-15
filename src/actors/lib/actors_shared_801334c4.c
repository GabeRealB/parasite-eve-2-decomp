#include "common.h"

#include "actors/actors_shared_801334c4.h"

#include "main/tmd.h"

s32 ActorsShared801334c4(Task* task, s32 arg1, ActorsShared801334c4Args* args)
{
    ActorsShared801334c4Coord* coord;
    TmdObject*                 extra;

    extra             = (TmdObject*)task->extra;
    coord             = (ActorsShared801334c4Coord*)extra->field_8;
    coord->coord.t[0] = args->pos.vx;
    coord->coord.t[1] = args->pos.vy;
    coord->coord.t[2] = args->pos.vz;
    coord->rot.vx     = args->rot.vx;
    coord->rot.vy     = args->rot.vy;
    coord->rot.vz     = args->rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg      = 0;
    extra->field_C &= 0xFF7F;
    return 0;
}
