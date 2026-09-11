#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_8016b370.h"

void ActorsShared8016b370(Task* arg0)
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
        work2            = (ActorsShared80168d3cWork*)arg0->idMap;
        work2->field_420 = 0;
        work2->field_422 = 0;
    }
}
