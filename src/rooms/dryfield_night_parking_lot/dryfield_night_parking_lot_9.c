#include "common.h"

#include "gameplay/3CD8.h"

/// Handler for message 0x13F0 in the room's message table: point 4 runs CAP
/// command 4. Always returns 0.
s32 func_dryfield_night_parking_lot_8017DB0C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 4) {
        Gp_RunCapCmd1(4);
    }
    return 0;
}
