#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_8013a0b0.h"

s32 ActorsShared8013a0b0(Task* arg0)
{
    ActorsShared8013a0b0Work* work = (ActorsShared8013a0b0Work*)arg0->idMap;

    if ((work->flags_FC.half & 1) || (work->flags_FC.word & 0x102)) {
        return 1;
    }
    return 0;
}
