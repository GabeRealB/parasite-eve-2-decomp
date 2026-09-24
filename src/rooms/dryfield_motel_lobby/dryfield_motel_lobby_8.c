#include "common.h"

#include "gameplay/1A8.h"

#include "main/task.h"

/// Location-message handler of the room's message table: copies the requested
/// location onto the outgoing record and answers 1.
s32 func_dryfield_motel_lobby_8017F414(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    return 1;
}
