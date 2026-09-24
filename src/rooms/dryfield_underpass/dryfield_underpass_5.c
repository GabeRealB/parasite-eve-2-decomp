#include "common.h"

#include "gameplay/3CD8.h"

#include "main/task.h"

/// Handler for message 0x13F2: when `arg2` is 2, queues stage sound 0x52260002.
/// Always returns 0.
s32 func_dryfield_underpass_8017D8CC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 2) {
        Gp_EnqueueStageSnd6(0x52260000 | 2, 0, 0);
    }
    return 0;
}
