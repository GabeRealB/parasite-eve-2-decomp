#include "common.h"

#include "actors/actors_shared_80162bc4.h"

#include "main/task.h"
#include "main/tmd.h"

s32 ActorsShared80162bc4(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject*                obj;
    ActorsShared80162bc4Work* work;
    s32                       ret;

    obj  = task->extra;
    work = (ActorsShared80162bc4Work*)task->work;
    ret  = 0;
    switch (mode) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags     |= 0x80;
            work->field_4C8 = mode;
            obj->flags     |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}
