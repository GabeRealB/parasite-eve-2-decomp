#include "common.h"

#include "actors/actors_shared_8013391c.h"

s32 ActorsShared8013391c(Task* task, s32 msgId, ActorsShared8013391cMsg* msg)
{
    ActorsShared8013391cWork* work;

    work = (ActorsShared8013391cWork*)task->work;
    switch (msg->field_2) {
        case 0:
            break;
        case 1:
            work->field_33A |= 1;
            break;
        case 2:
            work->field_33A |= 2;
            break;
        case 3:
            work->field_33A |= 3;
            break;
    }
    return 0;
}
