#include "common.h"

#include "main/tmd.h"

#include "actors/actors_shared_80169e64.h"

s32 ActorsShared80169e64(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*                obj;
    ActorsShared80169e64Work* work;

    obj  = (TmdObject*)task->extra;
    work = (ActorsShared80169e64Work*)task->idMap;
    switch (arg2) {
        case 0:
            obj->field_C = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            obj->field_C = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0x18;
            break;
        case 2:
            obj->field_C |= 4;
            work->field_0 = 0;
            break;
        case 3:
            obj->field_C  = 0;
            work->field_0 = 0;
            obj->field_C |= 4;
            break;
    }
    return 0;
}
