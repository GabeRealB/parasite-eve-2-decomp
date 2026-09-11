#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_8016b1c8.h"

void ActorsShared8016b1c8(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    s32                       cond;

    work = (ActorsShared80168d3cWork*)arg0->idMap;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work            = (ActorsShared80168d3cWork*)arg0->idMap;
        work->field_420 = 1;
        work->field_422 = 0;
    }
}
