#ifndef SRC_ROOMS_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_PRIVATE_H
#define SRC_ROOMS_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/message.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

/// Whether the latest staged-event gate call attempted to spawn its event (0 no, 1 yes).
///
/// Every call clears this byte, including queries. Execution raises it after
/// the unchecked spawn; the task does not clear it on completion.
extern s8 gShelterB1NorthMaintenanceWalkwayStagedEventStarted;

extern RoomFadeStorage gRoomEventFade;

/// Destination snapshot shared by this overlay's staged-event writer and task.
///
/// Acceptance copies all eight bytes; keep them stable and loaded through
/// CAP/sound playback until the staged task commits area, warp and room to
/// the live save and requests reload. A later accepted event replaces the snapshot.
extern RoomEventMsg gRoomEventStagedDestination;

/// Binds this room's staged reload destination to storage shared by its translation units.
///
/// Include before `room_events.h` in both translation units. The event writer
/// copies a complete `RoomEventMsg` into this lvalue before spawning; the staged
/// task reads its area, warp and room after CAP/sound playback. Keep the snapshot
/// unchanged and this overlay loaded until reload is requested. This binding
/// allocates no storage and has no side effects when evaluated.
#define ROOM_EVENT_STAGED_DESTINATION (gRoomEventStagedDestination)

extern RoomLatchedEvent gRoomEventLatched;

/// Queues a grey gouraud glow spanning the projected points `arg0[0]` and
/// `arg0[1]`, of radius `arg1` turned by the angle `arg2`.

/// Queues a red gouraud disc at the projected point `arg0`, of radius `arg1`.

#endif // SRC_ROOMS_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_PRIVATE_H
