#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_8016945c.h"
#include "actors/actors_shared_8016b500.h"

void ActorsShared8016b500(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    ActorsShared80168d3cWork* work2;
    ActorsShared80168d3cWork* work3;
    s32                       cond;

    work = (ActorsShared80168d3cWork*)arg0->idMap;
    if ((ActorsShared8016945c(arg0) << 0x10) == 0) {
        work2 = (ActorsShared80168d3cWork*)arg0->idMap;
        if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_412  = 0;
            work->field_438  = 1;
            work3            = (ActorsShared80168d3cWork*)arg0->idMap;
            work3->field_426 = 4;
            work3->field_41C = 0x10;
            work3->field_418 = 4;
            work3->field_414 = 1;
            work->field_422  = work->field_422 + 1;
        }
    }
}
