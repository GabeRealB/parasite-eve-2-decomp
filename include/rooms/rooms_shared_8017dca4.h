#ifndef ROOMS_SHARED_8017DCA4_H
#define ROOMS_SHARED_8017DCA4_H

#include "common.h"

#include "rooms/room_common.h"

/// Message gate shared by the day and night Saloon G & R. Copies the incoming
/// record to the outgoing one, then answers message 0xF: a clear `field_5`
/// writes the answer byte from the saloon's progress nibble, and a re-read of
/// `msgId` that still holds 0xF latches the request into the event gate, whose
/// answer is returned. Every other message simply answers 1.
s32 RoomsShared8017dca4(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);

#endif // ROOMS_SHARED_8017DCA4_H
