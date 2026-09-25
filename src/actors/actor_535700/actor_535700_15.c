#include "common.h"

#include "actors/actor_535700.h"

/// Message handler of the first enemy: message 0 arms the turn countdown
/// `field_4B4` at 0x14 frames, message 1 sets `field_4BC`, which turns the
/// footsteps on. Anything else does nothing.
s32 func_actor_535700_80132910(Task* task, s32 arg1, Actor535700Msg* msg)
{
    s32 kind;

    kind = msg->field_2;
    switch (kind) {
        case 0:
            D_actor_535700_80146844->field_4B4 = 0x14;
            break;
        case 1:
            D_actor_535700_80146844->field_4BC = kind;
            break;
    }
    return 0;
}
