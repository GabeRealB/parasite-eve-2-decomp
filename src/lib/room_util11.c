#include "common.h"

#include "gameplay/1A8.h"

#include "main/task.h"

/// Default location-message handler: copies the requested location onto the
/// outgoing record and answers 1, leaving the decision to whoever reads the
/// reply.
s32 Room_Util11(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    return 1;
}
