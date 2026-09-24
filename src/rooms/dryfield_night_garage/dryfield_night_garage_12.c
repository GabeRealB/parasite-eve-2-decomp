#include "common.h"

#include "gameplay/3CD8.h"

/// Room event callback: event 9 plays stage sound 0x52030009 and event 0x6C
/// reads the caption event key. Always returns 0.
s32 func_dryfield_night_garage_80180300(s32 arg0, s32 arg1, s32 arg2)
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
