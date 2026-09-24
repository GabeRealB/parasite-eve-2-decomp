#include "common.h"

#include "gameplay/1A8.h"
#include "main/task.h"

/// Handler for message 0x13F1 in the room's message table: the room takes no
/// action and answers 0.
s32 func_dryfield_night_warehouse_8017D5D0(void)
{
    return 0;
}

/// Handler for message 0x13EE in the room's message table: copies the location
/// record the sender passes onto the reply record and answers 1.
s32 func_dryfield_night_warehouse_8017D5D8(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    return 1;
}

/// Handler for message 0x13F0 in the room's message table: does nothing and
/// answers 0.
s32 func_dryfield_night_warehouse_8017D600(void)
{
    return 0;
}

/// Handler for message 0x13EF in the room's message table: does nothing and
/// answers 0.
s32 func_dryfield_night_warehouse_8017D608(void)
{
    return 0;
}
