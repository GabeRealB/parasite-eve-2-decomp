#include "common.h"

#include "gameplay/D4.h"

#include "main/task.h"
#include "rooms/rooms_shared_8017db1c.h"

s32 RoomsShared8017db1c(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 0x8:
            Gp_EnqueueStageSnd6(0x521D0008, 0, 0);
            break;
        case 0x9:
            Gp_EnqueueStageSnd6(0x521D0009, 0, 0);
            break;
    }
    return 0;
}
