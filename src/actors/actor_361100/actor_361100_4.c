#include "common.h"

#include "actors/actor_361100.h"

s32 func_actor_361100_80163670(Task* task, s32 arg1, s32 mode)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra;
    ret = 0;
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
            obj->field_C                              |= 0x80;
            ((Actor361100Work*)task->idMap)->field_4A2 = mode;
            obj->field_C                              |= 4;
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

s32 func_actor_361100_80163750(Task* task, s32 msgId, Actor361100Msg* msg)
{
    Actor361100Work* work;

    work = (Actor361100Work*)task->idMap;
    switch (msg->field_2) {
        case 0:
            work->field_480 = 0;
            work->field_484 = 0;
            work->field_488 = 0;
            work->field_4A0 = 0;
            break;
        case 1:
            work->field_484 = 0x2D000;
            work->field_480 = 0;
            work->field_488 = 0;
            work->field_4A0 = 0xA0;
            break;
        default:
            task->exitCallback(task);
            break;
    }
    return 0;
}
