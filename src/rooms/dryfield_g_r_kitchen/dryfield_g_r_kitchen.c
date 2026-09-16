#include "common.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d638.h"

s32 func_dryfield_g_r_kitchen_8017D8BC(void)
{
    return 0;
}

/// Room event-script handler: mirrors the incoming message onto the outgoing
/// one and, for message `0x14`, hands the event gate a request that fires on
/// game-flag nibble `0x34` (`field_0`/`field_4` 3, `field_8`/`field_C` the
/// stage-sound words `0x52130001` and `0x52130004`, no collected-item
/// prerequisite), answering with the gate's result. Every other message
/// answers 1.
s32 func_dryfield_g_r_kitchen_8017D8C4(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          ret;

    *out = *in;
    if (in->msgId == 0x14) {
        req.field_0 = 3;
        req.field_4 = 3;
        req.field_8 = 0x52130001;
        req.field_C = 0x52130004;
        req.flagId  = 0x34;
        req.itemId  = 0;
        ret         = RoomsShared8017d638(&req, in);
    } else {
        ret = 1;
    }
    return ret;
}

s32 func_dryfield_g_r_kitchen_8017D948(void)
{
    return 0;
}

s32 func_dryfield_g_r_kitchen_8017D950(void)
{
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_g_r_kitchen/dryfield_g_r_kitchen", RoomsShared8017d878Table);
