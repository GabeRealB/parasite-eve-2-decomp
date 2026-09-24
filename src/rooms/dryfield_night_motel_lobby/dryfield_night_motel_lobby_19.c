#include "common.h"

#include "main/gameflag.h"

#include "rooms/room_common.h"

/// Message handler: copies the incoming message onto the outgoing one and, for
/// message 0xF with `field_5` clear, answers in `field_3` with game-flag nibble
/// 0x61 plus one. Always returns 1.
s32 func_dryfield_night_motel_lobby_8017FB08(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->msgId == 0xF && in->field_5 == 0) {
        out->field_3 = GameFlag_GetNibble(0x61) + 1;
    }
    return 1;
}
