#include "common.h"

#include "main/gameflag.h"

#include "rooms/room_common.h"

/// Message 0x26: copies the incoming record onto the outgoing one and, when
/// not a report-only query (`field_5 == 0`), answers in `field_3` from nibbles
/// 0xC9 / 0x53 / 0x51. Always returns 1 (not consumed).
s32 Room_Script09(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
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
