#include "common.h"

#include "rooms/room_common.h"

extern s32 func_80179954(RoomEventMsg* in, RoomEventMsg* out);

/// Message handler that copies the incoming record onto the outgoing one and
/// forwards both to `func_80179954`. Always returns 1.
s32 func_dryfield_night_garage_80180360(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179954(in, out);
    return 1;
}
