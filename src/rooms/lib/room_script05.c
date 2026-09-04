#include "common.h"

#include "main/gameflag.h"

#include "rooms/room_common.h"

/// Message 0xF: copies the incoming record onto the outgoing one and, when not
/// a report-only query (`field_5 == 0`), answers with nibble 0x61 plus one.
/// Always returns 1 (not consumed).
s32 Room_Script05(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->msgId == 0xF && in->field_5 == 0) {
        out->field_3 = GameFlag_GetNibble(0x61) + 1;
    }
    return 1;
}
