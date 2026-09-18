#include "common.h"

#include "actors/actors_shared_8013d268.h"

#include "main/tmd.h"

s32 ActorsShared8013d268(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*                obj;
    ActorsShared8013d268Work* work;

    obj  = (TmdObject*)task->extra;
    work = (ActorsShared8013d268Work*)task->work;
    switch (arg2) {
        case 0:
            obj->flags = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            break;
        case 2:
            obj->flags   |= 4;
            work->field_0 = 0;
            break;
        case 3:
            obj->flags    = 0;
            work->field_0 = 0;
            obj->flags   |= 4;
            break;
    }
    return 0;
}
