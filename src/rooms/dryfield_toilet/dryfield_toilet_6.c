#include "common.h"

#include "main/gameflag.h"

#include "rooms/room_common.h"

/// Message handler that answers query 0xF: copies the incoming record onto the
/// outgoing one and, unless the query is report-only (`field_5` set), replies
/// with game flag nibble 0x61 plus one. Always returns 1, leaving the message
/// unconsumed.
s32 func_dryfield_toilet_8017D810(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->msgId == 0xF && in->field_5 == 0) {
        out->field_3 = GameFlag_GetNibble(0x61) + 1;
    }
    return 1;
}
