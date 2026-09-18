#include "common.h"

#include "actors/actor_323400.h"

s32 func_actor_323400_80164974(Task* task, s32 arg1, Actor323400Msg* msg, s32 arg3)
{
    Actor323400Work*     work;
    Actor323400MsgBytes* bytes;
    u16                  mode;

    work  = (Actor323400Work*)task->work;
    bytes = (Actor323400MsgBytes*)msg;

    work->field_91C = bytes->b0;
    work->field_91D = bytes->b1;
    work->field_91E = bytes->b2;

    if (msg->code == 0x1602) {
        mode = msg->mode;
        switch (mode) {
            case 1:
                ((TmdObject*)task->extra)->field_8->coord.t[0] = 0x4330;
                ((TmdObject*)task->extra)->field_8->coord.t[1] = mode;
                ((TmdObject*)task->extra)->field_8->coord.t[2] = 0xA8C;
                ((TmdObject*)task->extra)->field_8->flg        = 0;
                work->field_0                                  = 2;
                break;
            case 0:
            case 2:
                work->field_0 = 0;
                break;
        }
    }
    return 0;
}

INCLUDE_RODATA("actors/nonmatchings/actor_323400/actor_323400_3", jtbl_actor_323400_80161E44);
