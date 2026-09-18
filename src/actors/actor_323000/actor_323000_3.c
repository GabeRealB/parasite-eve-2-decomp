#include "common.h"

#include "actors/actor_323000.h"

s32 func_actor_323000_80164A54(Task* task, s32 arg1, Actor323000Msg* msg, s32 arg3)
{
    Actor323000Work*     work;
    Actor323000MsgBytes* bytes;

    work  = (Actor323000Work*)task->work;
    bytes = (Actor323000MsgBytes*)msg;

    work->field_91C = bytes->b0;
    work->field_91D = bytes->b1;
    work->field_91E = bytes->b2;

    if (msg->code == 0x202) {
        switch (msg->mode) {
            case 1:
                work->field_0 = 2;
                break;
            case 0:
            case 2:
                work->field_0 = 0;
                break;
            case 3:
                work->field_0 = msg->mode;
                break;
        }
    }
    return 0;
}
