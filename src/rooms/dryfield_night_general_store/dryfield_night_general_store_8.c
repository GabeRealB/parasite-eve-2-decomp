#include "common.h"

#include "gameplay/3CD8.h"
#include "main/task.h"

/// Message handler that plays stage sound 0x52030007 on action 7. Always
/// returns 0.
s32 func_dryfield_night_general_store_8017DDF0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 7) {
        Gp_EnqueueStageSnd6(0x52030000 | 7, 0, 0);
    }
    return 0;
}
