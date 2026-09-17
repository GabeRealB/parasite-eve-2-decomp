#include "common.h"

#include "actors/actor_323000.h"

s32 func_actor_323000_80164A54(Task* task, s32 arg1, Actor323000Msg* msg, s32 arg3)
{
    Actor323000Work*     work;
    Actor323000MsgBytes* bytes;

    work  = (Actor323000Work*)task->idMap;
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

INCLUDE_RODATA("actors/nonmatchings/actor_323000/actor_323000_3", jtbl_actor_323000_80161E44);
