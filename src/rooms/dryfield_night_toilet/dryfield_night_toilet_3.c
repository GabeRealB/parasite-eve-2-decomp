#include "common.h"

#include "main/gameflag.h"

#include "rooms/room_common.h"

/// Message-table handler for id 0x13EE: echoes the incoming record into the
/// reply and, for a message 0xF that is not report-only (`field_5 == 0`),
/// answers game nibble 0x61 plus one. Returns 1.
s32 func_dryfield_night_toilet_8017D5D0(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->msgId == 0xF && in->field_5 == 0) {
        out->field_3 = GameFlag_GetNibble(0x61) + 1;
    }
    return 1;
}
