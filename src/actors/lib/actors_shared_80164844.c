#include "common.h"

#include "actors/actors_shared_80164844.h"

#include "main/tmd.h"

s32 ActorsShared80164844(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*                obj;
    ActorsShared80164844Work* work;

    obj  = (TmdObject*)task->extra;
    work = (ActorsShared80164844Work*)task->work;
    switch (arg2) {
        case 0:
            obj->field_C = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            obj->field_C = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = 2;
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
