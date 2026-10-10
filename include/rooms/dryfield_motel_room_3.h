#ifndef INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_3_H
#define INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_3_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Motel room 3's room-1 collision mesh, view boundaries and exit trigger.
///
/// Index by room minus one. The eleven view boundaries and single action quad
/// include their LAST-marked records; there are no sight occluders. The stage
/// directory borrows this array while the overlay is loaded. Its mutable grid
/// and trigger storage must remain live until the active resources are retired.
extern WorldCollisionRoomResources gDryfieldMotelRoom3RoomCollisionResources[1];

/// Motel room 3's room-1 identity mapping of logical views to resource slots.
///
/// Index by room minus one, then logical view minus one in `gViewIdentityMap`.
/// Views 1..8 select the eight camera, image and sprite slots unchanged.
/// The room directory lives with this overlay; the shared byte map lives with
/// gameplay. The reverse-search limit is independent of both extents.
extern u8* gDryfieldMotelRoom3RoomViewMaps[1];

/// Motel room 3's six-entry reverse-view-search limit for room 1.
///
/// Index by room minus one. This signed-halfword count limits logical lookup
/// to views 1..6 even though forward mapping and view boundaries reach slots
/// 7 and 8. It is not the camera or sprite array length. Keep this overlay
/// loaded while the stage directory borrows the count.
extern ViewCount gDryfieldMotelRoom3RoomViewCounts[1];

/// Motel room 3's room-1 point-light collection with default ambient minima.
///
/// Index by room minus one. Four point lights contribute in all views; there
/// are no directional or cone lights and no per-view ambient table. The record
/// and mutable light transforms remain borrowed while this overlay is loaded.
extern WorldCoordRoomLighting gDryfieldMotelRoom3RoomLighting[1];

/// Motel room 3's sole arrival and departure endpoint at the main-street door.
///
/// Indexed by warp ID minus one; only ID 1 is valid. Player and companion
/// arrive at (4462, 0, 1067), facing yaw 3072 in 4096 units per turn, in view 2.
/// Arrival/departure play this area's script entries 2/1; blocking is silent.
/// No spawn or fade flags are set. Map marker 484 is shared with the matching
/// main-street endpoint. Whole-record copies require the overlay to be loaded.
extern DirectionWarpEntry gDryfieldMotelRoom3WarpEntries[1];

/// Motel room 3's eight cameras, indexed by the mapped view minus one.
///
/// Rotation coefficients have 12 fractional bits; translations are negated
/// world-space camera origins and projection distances are pixels. Keep this
/// overlay loaded until any queued camera application has copied its record.
extern ViewCamera gDryfieldMotelRoom3ViewCameras[8];

/// Motel room 3's eight sprite descriptors, retaining decoded background strips.
///
/// Index by the mapped view minus one. Each view has a zero-count background
/// record followed by a batch terminator, no source sprites and no clipping
/// areas. The unused source slot retains that view's batch-list address.
/// Descriptors and mutable batch lists remain borrowed while the overlay is loaded.
extern SpriteView gDryfieldMotelRoom3SpriteViews[8];

/// Motel room 3's collision and footstep policies for surface classes 0..7.
///
/// Classes 0 and 4..7 share silent, blocking, impact-enabled properties.
/// Classes 1 and 2 add distinct footstep cues; class 3 passes probes and ignores
/// weapon impacts without cues. Every class applies pushback. The mesh uses
/// classes 0..2, but room setup reads all eight entries. The directory and its
/// borrowed records stay valid only while this overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldMotelRoom3SurfacePropertiesByClass[8];

/// Runs the motel room 3's room-message receiver.
///
/// Start the borrowed task in state 0 to install this overlay's message table
/// and register `GAME_TASK_SLOT_ROOM`; state 1 idles and state 2 releases it.
/// The state index must be 0..2; dispatch is unchecked. Keep this overlay and
/// the registered task live while messages can arrive. Registration borrows
/// the task pointer and does not clear the slot on teardown.
void dryfieldMotelRoom3Task(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_3_H
