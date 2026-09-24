#include "common.h"

#include "rooms/room_common.h"

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// A handler of the room's message table: copies the incoming record onto the
/// outgoing one, hands both to `func_80179A04` and returns 1.
s32 func_mine_refuge_8017FBE8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    return 1;
}
