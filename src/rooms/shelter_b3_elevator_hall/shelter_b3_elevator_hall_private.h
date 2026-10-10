#ifndef SRC_ROOMS_SHELTER_B3_ELEVATOR_HALL_SHELTER_B3_ELEVATOR_HALL_PRIVATE_H
#define SRC_ROOMS_SHELTER_B3_ELEVATOR_HALL_SHELTER_B3_ELEVATOR_HALL_PRIVATE_H

#include "types.h"

#include "gameplay/message.h"

#include "rooms/room_common.h"

/// Snapshot of the accepted transition, copied by the gate for the deferred event.
///
/// All eight bytes are replaced on another accepted start. Keep this room's
/// snapshot unchanged and loaded until the task commits its destination.
extern RoomEventMsg gRoomEventMsg;

/// Selects this overlay's externally linked transition snapshot for the shared event code.
#define ROOM_EVENT_MSG gRoomEventMsg

extern RoomEventActiveBytes gRoomEventActive;
extern RoomEventReq         gRoomEventReq;

#endif // SRC_ROOMS_SHELTER_B3_ELEVATOR_HALL_SHELTER_B3_ELEVATOR_HALL_PRIVATE_H
