#include "common.h"

#include "actors/actors_shared_8013268c.h"

#include "main/task.h"

/// Message 0x7DB handler shared by actor_451100 and actor_260500: a zero
/// payload halfword arms the published block's `field_4B4` countdown at 0x14.
s32 ActorsShared8013268c(Task* task, s32 arg1, ActorsShared8013268cMsg* msg)
{
    if (msg->field_2 == 0) {
        ActorsShared80131f9cWork->field_4B4 = 0x14;
    }
    return 0;
}
