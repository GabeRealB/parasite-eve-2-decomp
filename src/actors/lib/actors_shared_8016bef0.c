#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_8016bef0.h"

s32 ActorsShared8016bef0(Task* arg0)
{
    ActorsShared80168d3cWork* work = (ActorsShared80168d3cWork*)arg0->idMap;

    if (work->field_41E == 1) {
        switch (work->field_448) {
            case 3:
                work->field_420 = 8;
                work->field_422 = 0;
                break;
            case 5:
                work->field_420 = 9;
                work->field_422 = 0;
                break;
        }
        work->field_448 = 0;
        return 1;
    }
    return 0;
}
