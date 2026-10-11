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

/// Selects the B3 elevator hall's retained destination for deferred room events.
///
/// Include before `room_events.h` in both room translation units. The binding
/// is a writable `RoomEventMsg` lvalue with no side effects; it allocates no
/// storage. The gate replaces all eight bytes on eligible execution, even if
/// task allocation fails; queries leave them unchanged. Keep the snapshot
/// loaded and unchanged until the task commits area/warp/room and requests reload.
#define ROOM_EVENT_DESTINATION (gRoomEventMsg)

extern RoomEventActiveBytes gRoomEventActive;
extern RoomEventReq         gRoomEventReq;

#endif // SRC_ROOMS_SHELTER_B3_ELEVATOR_HALL_SHELTER_B3_ELEVATOR_HALL_PRIVATE_H
