#include "common.h"

#include "actors/actor_151000.h"
#include "main/task.h"

/// Message handler: message 0 arms the turn countdown `turnFrames` at 0x14
/// frames, message 1 sets `footsteps`, which turns the footsteps on. Anything else does nothing.
s32 func_actor_151000_8013288C(Task* task, s32 arg1, Actor151000Msg* msg)
{
    s32 kind;

    kind = msg->field_2;
    switch (kind) {
        case 0:
            D_actor_151000_8013D37C->turnFrames = 0x14;
            break;
        case 1:
            D_actor_151000_8013D37C->footsteps = kind;
            break;
    }
    return 0;
}
