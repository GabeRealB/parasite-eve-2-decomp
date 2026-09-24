#include "common.h"

#include "rooms/room_common.h"

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// The room's handler for message 0x13EE: copies the incoming record onto the
/// outgoing one, passes both to `func_80179A04` and returns 1.
s32 func_shelter_r49_8017D5F4(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    return 1;
}
