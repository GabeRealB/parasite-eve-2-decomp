#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_8016b104.h"

extern u32 Gp_LcgState;

void ActorsShared8016b104(Task* arg0)
{
    ActorsShared80168d3cWork* work = (ActorsShared80168d3cWork*)arg0->idMap;
    s16                       dist;

    if (work->field_446 < (s16)work->field_412++) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            ActorsShared80168d3cWork* w = (ActorsShared80168d3cWork*)arg0->idMap;

            w->field_420 = 4;
            w->field_422 = 0;
        } else {
            ActorsShared80168d3cWork* w = (ActorsShared80168d3cWork*)arg0->idMap;

            w->field_420 = 1;
            w->field_422 = 0;
        }
        return;
    }
    dist = work->field_43A;
    if (dist < 0xDAC) {
        ActorsShared80168d3cWork* next = (ActorsShared80168d3cWork*)arg0->idMap;

        next->field_420 = 3;
        next->field_422 = 0;
        return;
    }
    if (dist < 0x1388) {
        work->field_422++;
    }
}
