#include "common.h"

#include "actors/actors_shared_8013be64.h"

#include "main/tmd.h"

s32 ActorsShared8013be64(Task* task, s32 msgId, s32 arg2)
{
    TmdObject* tmd;

    tmd = (TmdObject*)task->extra;
    if (arg2 == 0) {
        tmd->field_C = 0x80;
    } else {
        tmd->field_C = 0;
    }
    return 0;
}
