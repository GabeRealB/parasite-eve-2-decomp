#ifndef ROOMS_SHARED_8017D8BC_H
#define ROOMS_SHARED_8017D8BC_H

#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

/// Message handler shared by the day and night motel balconies. Copies the
/// incoming message, updates its response byte, and builds the request for
/// the balcony event gate. Returns 1 for messages outside its event cases,
/// otherwise the gate result with 0 changed to 2.
s32 RoomsShared8017d8bc(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out);

#endif // ROOMS_SHARED_8017D8BC_H
