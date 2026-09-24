#include "common.h"

#include "main/gameflag.h"
#include "rooms/room_common.h"

/// Room event handler: copies the incoming record onto the outgoing one and,
/// for message 0x1D with `field_5` clear, answers 1 or 3 in `field_3`
/// depending on whether flag nibble 0x61 is set. Always returns 1.
s32 func_dryfield_night_motel_room_6_80181B7C(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 nib;

    *out = *in;
    if (in->msgId == 0x1D && in->field_5 == 0) {
        nib = GameFlag_GetNibble(0x61);
        if (nib == 0) {
            nib = 1;
        } else {
            nib = 3;
        }
        out->field_3 = nib;
    }
    return 1;
}
