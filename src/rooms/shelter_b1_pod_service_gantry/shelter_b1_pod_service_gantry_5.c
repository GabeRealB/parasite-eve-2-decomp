#include "common.h"

#include "rooms/room_common.h"

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// Handler of message 0x13EE in the room's message table: copies the incoming
/// record onto the outgoing one, passes both to `func_80179A04` and returns 1.
s32 func_shelter_b1_pod_service_gantry_8017D7C8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    return 1;
}
