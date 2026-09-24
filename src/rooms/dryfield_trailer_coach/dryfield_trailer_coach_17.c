#include "common.h"

#include "gameplay/1A8.h"
#include "main/task.h"

/// Location-message handler: copies the requested location onto the outgoing
/// record and answers 1.
s32 func_dryfield_trailer_coach_80182580(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    return 1;
}
