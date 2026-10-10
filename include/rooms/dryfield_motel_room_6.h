#ifndef INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_6_H
#define INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_6_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_dryfield_motel_room_6_80182D0C[2];

extern AreaVariant D_dryfield_motel_room_6_80186764[12];

// dryfield_motel_room_6
extern WorldCollisionRoomResources D_dryfield_motel_room_6_80182D98[];

/// Logical-view maps for daytime motel room 6's single room.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1..12 select the same
/// camera, image and sprite indices. The directory has no terminator and
/// remains valid only while this room overlay is loaded.
extern u8* gDryfieldMotelRoom6RoomViewMaps[1];

extern ViewCount D_dryfield_motel_room_6_80182DAC[];

/// Authored lights and per-view ambient minima for daytime motel room 6.
///
/// Room 1 borrows mutable point/cone lights and ambient entries for views
/// 1..12. The collection, transforms and ambient table belong to this overlay
/// and must remain loaded while queried or composed.
extern WorldCoordRoomLighting gDryfieldMotelRoom6RoomLighting[1];

extern DirectionWarpEntry D_dryfield_motel_room_6_80182DB8[];

extern ViewCamera D_dryfield_motel_room_6_80183840[];

extern SpriteView D_dryfield_motel_room_6_801856CC[];

/// Collision policies and footstep cues for daytime motel room 6's surface classes.
///
/// Indexed directly by class 0..7, without a terminator. All classes block
/// probes, allow weapon impacts and apply pushback. Classes 1..3 select
/// distinct cue records; classes 0 and 4..7 share a record with no footsteps.
/// The directory and borrowed records stay valid while this overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldMotelRoom6SurfacePropertiesByClass[8];

/// Updates daytime motel room 6's telephone save and statistics menu.
///
/// Gameplay dispatches this wrapper while this room overlay is loaded.
/// Requires a live menu task in state 0..3, its writable UiObject in
/// `spawnArg2.pointer`, and loaded UI resources. The parent owns the menu;
/// the implementation opens child dialogs/panels and reports dismissal through
/// the object's result rather than killing this task.
void dryfieldMotelRoom6TelephoneMenuTask(Task* task);

/// Initializes and updates the player's reflection in daytime motel room 6.
///
/// State must be 0 (start bodyless) or 1 (initialized model and work).
/// `spawnArg1.value` selects the floor (0) or room mirror plane (1).
/// Requires a live player with a TMD body. The task owns its cloned model and
/// work, borrows the player's geometry, and is parented to the player.
/// Keep this overlay loaded while the reflection and its attachment tasks live.
void dryfieldMotelRoom6PlayerReflectionTask(Task* reflectionTask);

/// Draws daytime motel room 6's pulsing cyan glow at its fixed world point.
///
/// Mapped view indices 3 and 4 select a wide diamond; 12 selects layered discs
/// and blades. Other mapped views draw nothing. The task argument is unused.
/// Requires the current view matrix, initialized scratch stack and frame packet
/// arena and ordering table; accepted projections must have nonzero camera Z / 4.
/// Queued packets belong to the frame arena until GPU completion. Keep this
/// room overlay loaded while the effect task is live.
void dryfieldMotelRoom6DrawGlowTask(Task* unusedTask);

/// Runs daytime motel room 6's receiver through initialization, idle and teardown.
///
/// Requires a live bodyless task with state 0 (register), 1 (idle) or 2 (kill).
/// Initialization registers `GAME_TASK_SLOT_ROOM` and permits post-CAP sound
/// messages. Keep the room overlay loaded for the task's lifetime.
void dryfieldMotelRoom6Task(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_6_H
