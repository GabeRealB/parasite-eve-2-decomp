#include "common.h"

#include "actors/actor_361100.h"

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_4", func_actor_361100_80163670);

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
