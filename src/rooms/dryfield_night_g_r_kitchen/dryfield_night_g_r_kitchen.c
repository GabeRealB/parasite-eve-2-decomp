#include "common.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d638.h"

s32 func_dryfield_night_g_r_kitchen_8017D8BC(void)
{
    return 0;
}

/// Handler for the room's `0x14` message: copies the incoming payload through
/// to `out` and, when the message id matches, offers the gate a request that
/// plays this room's pair of stage sounds under flag nibble 0x34. Returns 1
/// for a message it does not own.
s32 func_dryfield_night_g_r_kitchen_8017D8C4(s32 arg0, s32 arg1, RoomEventMsg* in,
                                             RoomEventMsg* out)
{
    RoomEventReq req;

    *out = *in;
    if (in->msgId == 0x14) {
        req.field_0 = 3;
        req.field_4 = 3;
        req.field_8 = 0x53130001;
        req.field_C = 0x53130004;
        req.flagId  = 0x34;
        req.itemId  = 0;
        return RoomsShared8017d638(&req, in);
    }
    return 1;
}

s32 func_dryfield_night_g_r_kitchen_8017D948(void)
{
    return 0;
}

s32 func_dryfield_night_g_r_kitchen_8017D950(void)
{
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_g_r_kitchen/dryfield_night_g_r_kitchen", RoomsShared8017d878Table);
