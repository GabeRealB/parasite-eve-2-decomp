#include "common.h"

#include "main/task.h"

#include "rooms/room_common.h"

/// Message-table handler for id 0x13F1: accepts the message and does nothing.
s32 func_dryfield_motel_loft_8017D5D0(void)
{
    return 0;
}

/// Message-table handler for id 0x13EE: echoes the incoming record into the
/// reply unchanged and returns 1.
s32 func_dryfield_motel_loft_8017D5D8(Task* task, s32 msgId, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    return 1;
}

/// Message-table handler for id 0x13F0: accepts the message and does nothing.
s32 func_dryfield_motel_loft_8017D600(void)
{
    return 0;
}

/// Message-table handler for id 0x13EF: accepts the message and does nothing.
s32 func_dryfield_motel_loft_8017D608(void)
{
    return 0;
}
