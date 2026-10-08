#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_6_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_6_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_dryfield_night_motel_room_6_80182E74[2];

extern AreaVariant D_dryfield_night_motel_room_6_801861B4[11];

// dryfield_night_motel_room_6
extern WorldCoordRoomLighting D_dryfield_night_motel_room_6_80182F00[];

extern WorldCollisionRoomResources D_dryfield_night_motel_room_6_80182F08[];

extern u8* D_dryfield_night_motel_room_6_80182F18[];

extern ViewCount D_dryfield_night_motel_room_6_80182F1C[];

extern DirectionWarpEntry D_dryfield_night_motel_room_6_80182F20[];

extern ViewCamera D_dryfield_night_motel_room_6_801839A8[];

extern SpriteView D_dryfield_night_motel_room_6_801857C0[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_motel_room_6_80186250[];

/// Updates nighttime motel room 6's telephone save and statistics menu.
///
/// The UI task owns the live `UiObject` in `spawnArg2.pointer`, including its
/// save, notice and statistics children. Start in state 0; states 1..3 track
/// those child dialogs. Cancel clears the session's UI-open flag and publishes
/// a cancel result. Keep this room overlay loaded until the UI tree closes.
void dryfieldNightMotelRoom6TelephoneMenuTask(Task* task);

/// Initializes and updates the player's reflection in nighttime motel room 6.
///
/// State must be 0 (start bodyless) or 1 (initialized model and work).
/// `spawnArg1.value` selects the floor (0) or room mirror plane (1).
/// Requires a live player with a TMD body. The task owns its cloned model and
/// work, borrows the player's geometry, and is parented to the player.
/// Keep this overlay loaded while the reflection and its attachment tasks live.
void dryfieldNightMotelRoom6PlayerReflectionTask(Task* reflectionTask);

/// Draws nighttime motel room 6's pulsing cyan glow at its fixed world point.
///
/// Mapped view indices 3 and 4 select a wide diamond; 12 selects layered discs
/// and blades. Other mapped views draw nothing. The task argument is unused.
/// Requires the current view matrix, initialized scratch stack and frame packet
/// arena and ordering table; accepted projections must have nonzero camera Z / 4.
/// Queued packets belong to the frame arena until GPU completion. Keep this
/// room overlay loaded while the effect task is live.
void dryfieldNightMotelRoom6DrawGlowTask(Task* unusedTask);

/// Runs nighttime motel room 6's room controller and message endpoint.
///
/// `state` must be 0 (register task and message table), 1 (idle) or 2 (kill).
/// Needs no body and ignores spawn payloads. The state table and callbacks are
/// borrowed from this overlay, which must remain loaded for the task's lifetime.
void dryfieldNightMotelRoom6RoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_6_H
