#include "common.h"

#include "gameplay/3CD8.h"

#include "main/task.h"

/// Message-table handler for id 0x13F2: on event 5 queues stage sound
/// 0x52100005. Returns 0.
s32 func_dryfield_night_toilet_8017D644(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 5) {
        Gp_EnqueueStageSnd6(0x52100000 | 5, 0, 0);
    }
    return 0;
}
