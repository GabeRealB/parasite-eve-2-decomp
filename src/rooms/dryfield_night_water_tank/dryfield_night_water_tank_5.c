#include "common.h"

#include "gameplay/1A8.h"
#include "main/task.h"

/// Handler for message 0x13EE in the room's message table: copies the location
/// record it is handed onto the outgoing one and answers 1.
s32 func_dryfield_night_water_tank_8017D714(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    return 1;
}
