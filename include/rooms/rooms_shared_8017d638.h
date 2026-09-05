#ifndef ROOMS_SHARED_8017D638_H
#define ROOMS_SHARED_8017D638_H

#include "common.h"

#include "main/task.h"

#include "rooms/room_common.h"

/// The room's pending event request and message, the flag saying one was
/// queued, and the task descriptor the gate spawns to run it. Every carrying
/// room holds its own four at its own addresses, named there by the family's
/// symbol maps, so the shared object owns no data.
extern RoomEventMsg RoomsShared8017d638Msg;
extern RoomEventReq RoomsShared8017d638Req;
extern u8           RoomsShared8017d638Flag;
extern TaskDesc     RoomsShared8017d638Desc;

/// The rooms' event gate: given a request and the incoming message, answers
/// whether the event fires. A set flag nibble (or a clear one, for a negative
/// `flagId`) means the event has already happened and the answer is 1; a
/// missing collected-bit prerequisite runs the request's CAP command and
/// answers 0; otherwise the request is latched into the room's pending pair,
/// the flag nibble is written and the room's event task is spawned, for 2.
/// A non-zero `field_5` on the message asks what *would* happen and suppresses
/// all of that. Twenty-eight rooms carry this body.
s32 RoomsShared8017d638(RoomEventReq* req, RoomEventMsg* msg);

#endif // ROOMS_SHARED_8017D638_H
