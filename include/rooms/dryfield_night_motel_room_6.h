#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_6_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_6_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Player and held-model reflection recipes for nighttime motel room 6.
///
/// Slots 0 and 1 start bodyless tasks at priority `TASK_PRIORITY_EFFECT`, without a terminator.
/// Player `spawnArg1.value` = 1 selects the room mirror plane; attachment
/// `spawnArg1.value` selects player-held slot 0..3 and `spawnArg2.pointer` borrows
/// the player reflection task. Spawning reads the recipe synchronously; keep
/// this overlay and the borrowed player geometry loaded while the tasks live.
extern TaskDesc gDryfieldNightMotelRoom6ReflectionTaskDescs[2];

/// Eleven absent placement/resource variants for nighttime motel room 6.
///
/// Indexed directly by `GameLocationKey.variant` (0..10), without a terminator.
/// Every slot contains two NULL pointers, so no placement actors or files are
/// selected. Gameplay borrows this table for night area 30 while the room is loaded.
extern AreaVariant gDryfieldNightMotelRoom6AreaVariants[11];

/// Lighting for nighttime motel room 6, indexed by the 1-based room ID minus one.
///
/// Only room 1 exists. It borrows five mutable point lights in every view and
/// has no ambient table, so lighting uses the gameplay default ambient minima.
/// Keep this overlay loaded while its lighting records are borrowed.
extern WorldCoordRoomLighting gDryfieldNightMotelRoom6RoomLighting[1];

/// Collision resources for nighttime motel room 6's sole room, room ID 1.
///
/// Indexed by room ID minus one, without a terminator. Borrows the mutable
/// grid, ten directed view boundaries and fifteen action triggers; there are
/// no sight occluders. Keep the overlay loaded while its grid is active or its
/// triggers are linked; room binding replaces their coordinates and list links.
extern WorldCollisionRoomResources gDryfieldNightMotelRoom6RoomCollisionResources[1];

/// Logical-view maps for nighttime motel room 6, indexed by room ID minus one.
///
/// Only room 1 exists and borrows `gViewIdentityMap`: logical views 1..12 map
/// to the same camera, image and sprite indices. The directory has no sentinel;
/// keep the room and gameplay overlays loaded while it is used.
extern u8* gDryfieldNightMotelRoom6ViewMaps[1];

/// Logical-view reverse-search count for nighttime motel room 6's sole room.
///
/// Room ID 1 selects the twelve-entry search at index zero. This signed element
/// count is independent of `gViewIdentityMap`'s capacity. There is no terminator;
/// the map overlay borrows this count while the room overlay is loaded.
extern ViewCount gDryfieldNightMotelRoom6ViewCounts[1];

/// Balcony and water-tower warp endpoints for nighttime motel room 6.
///
/// Warp IDs 1 and 2 select room views 2 and 5 at index ID minus one. Each places
/// player and companion at the same room-coordinate point, with yaw in 4096
/// units per turn. The balcony endpoint supplies arrival/departure sounds and
/// an arrival map mark; the water-tower departure fades. There is no terminator.
/// Consumers borrow or copy entries while the room overlay is loaded.
extern DirectionWarpEntry gDryfieldNightMotelRoom6WarpEntries[2];

/// Camera transforms and perspective distances for nighttime motel room 6's twelve views.
///
/// The identity view map selects entries at mapped view minus one, without a
/// terminator. Rotation uses `ONE` (4096) per unit, translation is the negated
/// world-space camera origin, and projection distance is in pixels. Keep the
/// room overlay loaded through lookup and any queued camera application.
extern ViewCamera gDryfieldNightMotelRoom6ViewCameras[12];

/// Sprite sources and drawing lists for nighttime motel room 6's twelve mapped views.
///
/// Indexed by mapped view minus one, without a terminator. Views 1, 6 and 9..12
/// retain empty batch-list addresses in their source slots; views 2..5, 7 and 8
/// select source sprites. Every batch list starts with a zero-count decoded-
/// background selector and ends at `SPRITE_BATCH_END`. No view supplies draw-area
/// clips. Keep this overlay loaded while accessing the mutable records or
/// allocating, updating and linking their cached sprite packets.
extern SpriteView gDryfieldNightMotelRoom6SpriteViews[12];

/// Collision policies and footstep cues for nighttime motel room 6's classes 0..7.
///
/// Every class blocks probes, permits weapon impacts and applies pushback.
/// Classes 1..3 have distinct footstep pairs; classes 0 and 4..7 share a silent
/// record. There is no sentinel. The map overlay borrows this directory and its
/// records only while the room overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldNightMotelRoom6SurfacePropertiesByClass[8];

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
