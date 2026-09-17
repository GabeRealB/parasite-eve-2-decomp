#ifndef ROOMS_SHARED_8017DAF8_H
#define ROOMS_SHARED_8017DAF8_H

#include "common.h"

#include "main/task.h"

#include "rooms/room_common.h"

/// Message handler shared by the two water towers (the day and night dryfield
/// rooms). Message 0x13 is the room's own event: it builds the gate request
/// for the tower scene and hands the gate the *incoming* message rather than
/// the copy, so the answer it returns is the gate's own -- 0 (a prerequisite
/// missing, the CAP command run) reported as 2. Message 0x15 asks whether the
/// scene it guards is still due: it clears the flag nibble that marks the
/// scene played and answers whether the chapter has moved on. Any other
/// message runs the first half of the 0x55 flag band's arming and reports 1.
///
/// Both rooms carry this body at their own address and reference their own
/// `RoomsShared8017d638Flag`, so the object owns no data.
s32 RoomsShared8017daf8(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out);

#endif // ROOMS_SHARED_8017DAF8_H
