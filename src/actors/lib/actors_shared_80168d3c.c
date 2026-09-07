#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actors_shared_80168d3c.h"

void ActorsShared80168d3c(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    ActorsShared80168d3cWork* work2;
    s16                       tmp;

    work             = (ActorsShared80168d3cWork*)arg0->idMap;
    work->field_92   = *(u16*)&((TmdObject*)arg0->extra)->field_8->coord.t[1];
    work2            = (ActorsShared80168d3cWork*)arg0->idMap;
    tmp              = 8;
    work2->field_426 = tmp;
    work2->field_418 = tmp;
    work2->field_41C = 0x10;
    tmp              = 1;
    work2->field_414 = tmp;
    work->field_412  = 0;
    work->field_428  = 0;
    work->field_42A  = -0x12C;
    work->field_440  = tmp;
    work->field_438  = 0;
    work->field_432  = 0;
    work->field_422  = work->field_422 + 1;
}
