#ifndef INCLUDE_ROOMS_DRYFIELD_MOTEL_LOBBY_H
#define INCLUDE_ROOMS_DRYFIELD_MOTEL_LOBBY_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Collision resources for the lobby's single room, indexed by room minus one.
///
/// Borrows the mutable grid, four view boundaries and seven action triggers;
/// there are no sight occluders. Keep the lobby overlay loaded while the grid
/// is active and until its triggers have been unlinked.
extern WorldCollisionRoomResources gDryfieldMotelLobbyRoomCollisionResources[1];

/// Logical-view map for the lobby's single room, indexed by room minus one.
///
/// Borrows `gViewIdentityMap`: logical views 1..5 select the same 1-based
/// camera, image and sprite indices. The room directory and gameplay map
/// must remain loaded during lookup.
extern u8* gDryfieldMotelLobbyRoomViewMaps[1];

/// Logical-view reverse-search count for the lobby's single room.
///
/// Indexed by room minus one. Searches five entries of the borrowed identity
/// map; this count is independent of that map's capacity.
extern ViewCount gDryfieldMotelLobbyRoomViewCounts[1];

/// Lighting for the lobby's single room, indexed by room minus one.
///
/// Borrows five mutable point lights and uses the default ambient minima.
/// The room overlay owns the collection and stays loaded through shading
/// and coordinate updates.
extern WorldCoordRoomLighting gDryfieldMotelLobbyRoomLighting[1];

/// The parking-lot door's arrival and departure descriptor, warp slot 1.
///
/// Both actors arrive at (2940, 0, 316), yaw zero, in lobby view 2. Uses the
/// lobby-bank arrival/departure sounds and map flag 481, with no blocked sound
/// or special warp flags. Lookup copies the full record from the loaded overlay.
extern DirectionWarpEntry gDryfieldMotelLobbyWarpEntries[1];

/// Cameras for the lobby's mapped views 1..5, stored at index minus one.
///
/// Rotations use Q12 coefficients, translations world units, and projection
/// distances pixels. A queued application borrows its camera until dispatch;
/// keep the lobby overlay loaded until that application completes.
extern ViewCamera gDryfieldMotelLobbyViewCameras[5];

/// Sprite descriptors for the lobby's mapped views 1..5, indexed by view minus one.
///
/// Views 2..4 supply 50, 52 and 71 depth-sorted rectangles; views 1 and 5
/// select none. Every view retains decoded image strips and has no draw-area
/// clips. The mutable sources and batches belong to the loaded lobby overlay.
extern SpriteView gDryfieldMotelLobbySpriteViews[5];

/// Collision policies and footstep cues for the lobby's surface classes 0..7.
///
/// Every class blocks probes, permits weapon impacts and applies pushback.
/// Class 1 is the main floor and supplies footstep cues; the other classes
/// share a silent record. Keep the lobby overlay loaded while using its records.
extern WorldCollisionSurfaceProperties* gDryfieldMotelLobbySurfacePropertiesByClass[8];

/// Updates the motel-lobby telephone's save menu and optional play statistics.
///
/// `task->spawnArg2.pointer` must hold its live UI object; save and statistics
/// children remain linked until their answers are consumed. Requires the lobby
/// overlay and telephone UI resources to remain loaded throughout the menu.
void dryfieldMotelLobbyTelephoneMenuTask(Task* task);

/// Dispatches the motel lobby's room-message task.
///
/// `task->state` must be 0 (register receiver), 1 (idle), or 2 (teardown);
/// dispatch does not check the index. State 0 borrows this overlay's message
/// table and registers the task in `GAME_TASK_SLOT_ROOM`, then advances to 1.
/// Keep the overlay loaded while the task can receive messages. Registration
/// does not retain the task, and teardown does not clear the session slot.
void dryfieldMotelLobbyTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MOTEL_LOBBY_H
