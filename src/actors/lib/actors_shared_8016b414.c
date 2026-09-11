#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_8016b414.h"

void ActorsShared8016b414(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    ActorsShared80168d3cWork* work2;
    s32                       cond;

    work = (ActorsShared80168d3cWork*)arg0->idMap;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_412  = 0;
        work->field_438  = 1;
        work2            = (ActorsShared80168d3cWork*)arg0->idMap;
        work2->field_426 = 4;
        work2->field_41C = 0x10;
        work2->field_418 = 4;
        work2->field_414 = 1;
        work->field_422++;
    }
}
