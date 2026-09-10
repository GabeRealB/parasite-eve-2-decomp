#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_8016974c.h"

s16 ActorsShared8016974c(Task* arg0)
{
    ActorsShared80168d3cWork* work = (ActorsShared80168d3cWork*)arg0->idMap;

    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        return 1;
    }
    return 0;
}
