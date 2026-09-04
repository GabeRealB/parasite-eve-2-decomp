#include "common.h"

#include "main/gameflag.h"

#include "rooms/room_common.h"

/// Message 0x1D: copies the incoming record onto the outgoing one and, when
/// not a report-only query (`field_5 == 0`), answers 1 or 3 from nibble 0x61.
/// Always returns 1 (not consumed).
s32 Room_Script04(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
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
