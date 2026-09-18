#include "common.h"

#include "actors/actor_113100.h"

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

s32 func_actor_113100_801333B8(Task* task, s32 msgId, Actor113100Msg7DB* msg)
{
    Actor113100Work* work;
    TmdObject*       model;

    work = (Actor113100Work*)task->work;

    switch (msg->field_2) {
        case 0:
            if (work->field_534 != NULL) {
                model           = (TmdObject*)work->field_534->extra;
                model->field_C &= 0xFF7F;
            }
            break;

        case 1:
            if (work->field_534 != NULL) {
                model           = (TmdObject*)work->field_534->extra;
                model->field_C |= 0x80;
            }
            break;

        case 2:
            work->field_53C = 1;
            break;

        case 3:
            work->field_53C = 0;
            break;

        default:
            return 0;
    }
    return 0;
}
