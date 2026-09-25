#include "common.h"

#include "actors/actor_260500.h"
#include "main/task.h"

/// Message 0x7DB: a zero payload halfword at 0x2 arms the work block's
/// `field_4B4` at 0x14.
s32 func_actor_260500_8014A818(Task* task, s32 arg1, Actor260500Msg* msg)
{
    if (msg->field_2 == 0) {
        D_actor_260500_80159E4C->field_4B4 = 0x14;
    }
    return 0;
}
