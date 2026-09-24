#include "common.h"

#include "rooms/room_common.h"

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// Message-table handler for message 0x13EE: copies the incoming record onto
/// the outgoing one and passes both on to `func_80179A04`. Always answers 1.
s32 func_shelter_r48_8017E044(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    return 1;
}
