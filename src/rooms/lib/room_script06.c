#include "common.h"

#include "main/gameflag.h"

#include "rooms/room_common.h"

/// Message 0x20 / 0x22: copies the incoming record onto the outgoing one and,
/// when not a report-only query (`field_5 == 0`), answers in `field_3` from
/// nibbles 0x51 / 0x53 / 0x52. Always returns 1 (not consumed).
s32 Room_Script06(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 val;

    *out = *in;
    USE_REG(out);
    USE_REG(out);
    if (in->msgId == 0x20 && in->field_5 == 0) {
        val = GameFlag_GetNibble(0x51);
        if (val == 0) {
            val = 2;
        } else {
            val = 1;
        }
        out->field_3 = val;
        if (GameFlag_GetNibble(0x53) != 0) {
            out->field_3 = (u8)out->field_3 + 2;
        }
    }
    if (in->msgId == 0x22 && in->field_5 == 0) {
        val = GameFlag_GetNibble(0x52);
        if (val == 0) {
            val = 2;
        } else {
            val = 1;
        }
        out->field_3 = val;
    }
    return 1;
}
