#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actors_shared_8013454c.h"
#include "actors/actors_shared_80134af4.h"

void ActorsShared80134af4(Task* task)
{
    GsCOORDINATE2*            coord;
    ActorsShared8013454cWork* work;

    coord              = ((TmdObject*)task->extra)->field_8;
    work               = (ActorsShared8013454cWork*)task->idMap;
    work->field_274    = coord->coord.t[0];
    work->field_278    = coord->coord.t[1];
    work->field_27C    = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_2BE) >> 12;
    coord->coord.t[1] += work->field_2DE;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_2BE) >> 12;
}
