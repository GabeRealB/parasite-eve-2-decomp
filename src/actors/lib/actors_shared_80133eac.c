#include "common.h"

#include "actors/actors_shared_80133eac.h"

#include "main/tmd.h"

s32 ActorsShared80133eac(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = (TmdObject*)task->extra;
    if (!(arg2 & 1)) {
        obj->field_C = 0x80;
    } else {
        obj->field_C = 0;
    }
    if (arg2 & 2) {
        obj           = (TmdObject*)task->extra;
        obj->field_C |= 4;
    }
    return 0;
}
