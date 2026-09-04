#include "common.h"

#include "rooms/room_common.h"

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// Default message handler: copies the incoming record onto the outgoing one
/// and forwards both to `func_80179A04`. Always returns 1 (not consumed).
s32 Room_Util02(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    return 1;
}
