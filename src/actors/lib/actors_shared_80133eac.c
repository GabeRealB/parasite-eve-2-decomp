#include "common.h"

#include "actors/actors_shared_80133eac.h"

#include "main/tmd.h"

s32 ActorsShared80133eac(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = (TmdObject*)task->extra;
    if (!(arg2 & 1)) {
        obj->flags = 0x80;
    } else {
        obj->flags = 0;
    }
    if (arg2 & 2) {
        obj         = (TmdObject*)task->extra;
        obj->flags |= 4;
    }
    return 0;
}
