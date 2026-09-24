#include "common.h"

#include "gameplay/1A8.h"
#include "main/task.h"

/// The room's 0x13F1 message handler: answers 0 without looking at the
/// message.
s32 func_dryfield_night_breezeway_8017D5D0(void)
{
    return 0;
}

/// The room's 0x13EE message handler: copies the incoming location onto the
/// outgoing record and answers 1.
s32 func_dryfield_night_breezeway_8017D5D8(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    return 1;
}
