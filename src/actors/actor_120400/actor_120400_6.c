#include "common.h"

#include "actors/actor_120400.h"

#include "main/task.h"
#include "main/tmd.h"

s32 func_actor_120400_80132C38(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject*           obj;
    Actor120400MainWork* work;
    s32                  ret;

    obj  = task->extra;
    work = (Actor120400MainWork*)task->idMap;
    ret  = 0;
    switch (mode) {
        case 0:
            obj->field_C |= 0x80;
            obj->field_C &= ~4;
            break;
        case 1:
            obj->field_C &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->field_C &= ~4;
            break;
        case 2:
            obj->field_C   |= 0x80;
            work->field_500 = mode;
            obj->field_C   |= 4;
            break;
        case 3:
            obj->field_C &= ~0x80;
            obj->field_C |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

s32 func_actor_120400_80132D14(void)
{
    return 0;
}
