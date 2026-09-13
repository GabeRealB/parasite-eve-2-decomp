#include "common.h"

#include "actors/actors_shared_801381e0.h"

/// Main-executable global with no module header yet: the remaining-enemy count.
extern s16 D_80073BA0;

s32 ActorsShared801381e0(Task* task)
{
    if (D_80073BA0 > 0) {
        ((ActorsShared801381e0Work*)task->idMap)->field_6F4 = 1;
    }
    return 0;
}
