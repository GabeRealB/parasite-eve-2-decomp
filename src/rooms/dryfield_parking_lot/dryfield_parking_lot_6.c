#include "common.h"

#include "gameplay/3CD8.h"

/// Handler for message 0x13F2 in the room's message table, keyed by `arg2`:
/// point 9 plays stage sound 0x520F0009 and point 10 plays 0x520F000A. Always
/// returns 0.
s32 func_dryfield_parking_lot_8017DAA0(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 9:
            Gp_EnqueueStageSnd6(0x520F0009, 0, 0);
            break;
        case 10:
            Gp_EnqueueStageSnd6(0x520F000A, 0, 0);
            break;
    }
    return 0;
}
