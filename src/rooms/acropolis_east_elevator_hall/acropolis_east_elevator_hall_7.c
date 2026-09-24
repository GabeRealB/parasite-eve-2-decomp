#include "common.h"

#include "gameplay/1A8.h"
#include "main/task.h"

/// Message handler that copies the incoming location record onto the
/// outgoing one and answers 1.
s32 func_acropolis_east_elevator_hall_8017F348(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    return 1;
}
