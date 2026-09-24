#include "common.h"

#include "main/gameflag.h"

#include "rooms/room_common.h"

/// The room's handler for message 0x13EE. It passes the message on unchanged,
/// except that message 2 with `field_5` clear gets its `field_3` rewritten:
/// 3 once game-flag nibble 0x7A has reached 4, otherwise one more than nibble
/// 0x61. Always reports the message handled.
s32 func_dryfield_night_motel_room_2_8017D5D8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 val;
    s32 n;

    *out = *in;
    if (in->msgId == 2 && in->field_5 == 0) {
        n = GameFlag_GetNibble(0x7A);
        if (n < 4) {
            val = 3;
            TOUCH_REG(val);
            val = GameFlag_GetNibble(0x61) + 1;
        } else {
            val = 3;
        }
        out->field_3 = val;
    }
    return 1;
}
