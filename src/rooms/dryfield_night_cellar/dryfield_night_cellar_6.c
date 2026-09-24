#include "common.h"

#include "main/gameflag.h"

#include "rooms/room_common.h"

/// Message-table handler for message 0x13EE. Copies the incoming record onto
/// the outgoing one; for a query 0x26 without `field_5` set it answers in
/// `field_3` from event nibbles 0xC9, 0x53 and 0x51 (1 to 4 while 0xC9 is set,
/// 5 or 6 otherwise). Always answers 1.
s32 func_dryfield_night_cellar_8017D634(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->msgId == 0x26 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0xC9) != 0) {
            if (GameFlag_GetNibble(0x53) != 0) {
                out->field_3 = 2;
            } else {
                out->field_3 = 1;
            }
            if (GameFlag_GetNibble(0x51) == 0) {
                out->field_3 = (u8)out->field_3 + 2;
            }
        } else {
            if (GameFlag_GetNibble(0x51) != 0) {
                out->field_3 = 5;
            } else {
                out->field_3 = 6;
            }
        }
    }
    return 1;
}
