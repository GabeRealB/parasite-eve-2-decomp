#include "common.h"

#include "main/gameflag.h"

#include "rooms/room_common.h"

/// Message-table handler for id 0x13EE: echoes the incoming record into the
/// reply and, for a message 2 that is not report-only (`field_5 == 0`),
/// answers game nibble 0x61 plus one while nibble 0x7A is below 4, and 3 once
/// it has reached 4. Returns 1.
s32 func_dryfield_night_motel_room_4_8017D5D8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
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
