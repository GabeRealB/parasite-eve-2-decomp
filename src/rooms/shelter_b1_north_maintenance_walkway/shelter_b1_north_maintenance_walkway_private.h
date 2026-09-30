#ifndef SRC_ROOMS_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_PRIVATE_H
#define SRC_ROOMS_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/message.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

extern RoomFadeStorage gRoomEventFade;

extern RoomEventMsg gRoomEventStagedMsg;

extern RoomLatchedEvent gRoomEventLatched;

/// Queues a grey gouraud glow spanning the projected points `arg0[0]` and
/// `arg0[1]`, of radius `arg1` turned by the angle `arg2`.

/// Queues a red gouraud disc at the projected point `arg0`, of radius `arg1`.

#endif // SRC_ROOMS_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_PRIVATE_H
