#include "common.h"

#include "rooms/room_common.h"

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// Message handler that copies the incoming record onto the outgoing one and
/// passes both to `func_80179A04`. Always returns 1.
s32 func_shelter_b2_elevator_8017DA64(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    return 1;
}
