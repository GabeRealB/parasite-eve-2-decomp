#include "common.h"
#include "gameplay/3CD8.h"

/// Handler for message 0x13F2 in the room's message table: on event 9 it plays
/// stage sound 0x52030009, on event 0x6C it reads the cap event key, and it
/// always reports the message as not handled.
s32 func_dryfield_garage_8017D8BC(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 0x9:
            Gp_EnqueueStageSnd6(0x52030009, 0, 0);
            break;
        case 0x6C:
            Gp_GetCapEventKey();
            break;
    }
    return 0;
}
