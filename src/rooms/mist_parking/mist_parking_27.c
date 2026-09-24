#include "common.h"

#include "gameplay/1A8.h"

#include "main/task.h"

/// Message handler that copies the location record it is given onto the
/// reply record and answers 1.
s32 func_mist_parking_801826C0(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    return 1;
}
