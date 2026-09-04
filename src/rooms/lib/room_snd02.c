#include "common.h"

#include "gameplay/3CD8.h"

#include "main/task.h"

s32 Room_Snd02(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 7) {
        Gp_EnqueueStageSnd6(0x52030000 | 7, 0, 0);
    }
    return 0;
}
