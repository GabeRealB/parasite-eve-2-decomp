#include "common.h"

#include "main/gameflag.h"

#include "rooms/room_common.h"

/// Message-table handler for id 0x13EE: echoes the incoming record into the
/// reply and, for a message 0x1D that is not report-only (`field_5 == 0`),
/// answers 1 while game nibble 0x61 is clear and 3 once it is set. Returns 1.
s32 func_dryfield_night_motel_loft_8017D600(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
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
