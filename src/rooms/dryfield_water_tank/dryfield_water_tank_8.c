#include "common.h"

#include "gameplay/1A8.h"

#include "main/task.h"

/// Handler for message 0x13EE in the room task's message table: copies the
/// location record the sender passes onto the reply record and answers 1.
s32 func_dryfield_water_tank_8017D7C4(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    return 1;
}
