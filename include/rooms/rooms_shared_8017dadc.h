#ifndef ROOMS_SHARED_8017DADC_H
#define ROOMS_SHARED_8017DADC_H

#include "common.h"

#include "rooms/room_common.h"

/// Message handler for the two ids the water hole answers, carried by both the
/// day and the night room. Handlers registered in a room's `(msgId, handler)`
/// table are passed the incoming record and an outgoing copy of it, and answer
/// by editing `field_3` of the copy; this one passes the record through
/// otherwise unchanged and always reports success.
///
/// - 0x19 picks the value out of `gGameSession->loc.stage`: 2 when that byte is 2
///   and progress nibble 0x3A has reached 2, 1 before it gets there, and nibble
///   0x61 plus one whenever the session byte is anything else.
/// - 0x26 is 2 or 1 by nibble 0x53, raised by two (so 4 or 3) while nibble 0x51
///   is clear; with nibble 0xC9 clear instead it is 6 or 5 by nibble 0x51.
///
/// A non-zero `field_5` suppresses both, as it does for every handler.
s32 RoomsShared8017dadc(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);

#endif // ROOMS_SHARED_8017DADC_H
