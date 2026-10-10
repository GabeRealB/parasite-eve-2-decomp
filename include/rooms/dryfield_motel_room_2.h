#ifndef INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_2_H
#define INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_2_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_motel_room_2_80180410[12];

/// Motel Room 2's room directory of logical-view maps.
///
/// The sole room entry borrows gameplay's `gViewIdentityMap`: views 1..6
/// select camera, image and sprite slots 1..6. Index by room minus one.
/// The directory lives with this overlay; the shared byte map lives with gameplay.
extern u8* gDryfieldMotelRoom2RoomViewMaps[1];

/// Motel Room 2's room-1 collision mesh, eight view boundaries, six actions and two sight occluders.
///
/// Index by the 1-based room minus one; only room 1 exists. Setup binds the grid
/// and triggers to the current view and links/enables both trigger lists and
/// the occluders through their included LAST records. Resources are mutable
/// and borrowed from this overlay; unlink them before unloading it.
extern WorldCollisionRoomResources gDryfieldMotelRoom2RoomCollisionResources[1];

/// Motel Room 2's six-view reverse-search limit for room 1.
///
/// Counts are signed halfwords indexed by room minus one. The second stored
/// count is zero; it supplies no additional room mapping or lighting record.
/// Keep this overlay loaded while the stage directory borrows the array.
extern ViewCount gDryfieldMotelRoom2RoomViewCounts[2];

/// Motel Room 2's room-1 light collection with default ambient minima.
///
/// Index by room minus one. Five point lights contribute in all views, with
/// writable transforms and attenuation; no directional or cone lights exist.
/// The record and its collection remain borrowed while this overlay is loaded.
extern WorldCoordRoomLighting gDryfieldMotelRoom2RoomLighting[1];

/// Motel Room 2's single warp endpoint, connecting to the main street's endpoint 4.
///
/// Warp ID 1 selects element 0. Player and companion arrive at (4666, 0, 2610)
/// in room game units, facing 3/4 turn (3072 of 4096), in view 2. Arrival and
/// departure use this area's sound entries 2 and 1; blocked requests are silent.
/// Later arrivals update map-marker nibble 485. Borrow while this overlay is loaded.
extern DirectionWarpEntry gDryfieldMotelRoom2WarpEntries[1];

/// Motel Room 2's six cameras, indexed by the mapped view minus one.
///
/// Rotations have 12 fractional bits; translations are negated camera origins
/// in world units, and projection distances are pixels. Keep the overlay loaded
/// until any queued camera application has copied the selected record.
extern ViewCamera gDryfieldMotelRoom2ViewCameras[6];

/// Motel Room 2's six sprite descriptors, indexed by the mapped view minus one.
///
/// View 1 has no source sprites; views 2..6 select 72, 84, 47, 83 and 79
/// rectangles. Every view retains decoded background strips. Only view 5 clips
/// drawing, restoring the full buffer at sorting depth 800. Descriptors and
/// their mutable source/batch arrays remain borrowed while this overlay is loaded.
extern SpriteView gDryfieldMotelRoom2SpriteViews[6];

/// Motel Room 2's collision and footstep policies for surface classes 0..7.
///
/// Classes 0, 5, 6 and 7 share blocking, impact-enabled, silent properties.
/// Classes 1/2 also have footstep cues; class 3 passes probes and ignores weapon
/// impacts without cues; class 4 passes probes with impacts and class 2's cues.
/// Every class applies pushback. The grid uses classes 0..3, but setup reads
/// all eight entries. Borrow these records only while this overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldMotelRoom2SurfacePropertiesByClass[8];

/// Inert room-effect callback for bank 6, slot 0xCA, selected for daytime Motel Room 2.
///
/// Ignores the task without drawing, advancing state or releasing resources.
/// Keep the Dryfield Motel Room 2 overlay loaded while this callback is scheduled.
void dryfieldMotelRoom2EffectNoopTaskCA(Task* unusedTask);

/// Runs the motel room 2's room-message receiver.
///
/// Start the borrowed task in state 0 to install this overlay's message table
/// and register `GAME_TASK_SLOT_ROOM`; state 1 idles and state 2 releases it.
/// The state index must be 0..2; dispatch is unchecked. Keep this overlay and
/// the registered task live while messages can arrive. Registration borrows
/// the task pointer and does not clear the slot on teardown.
void dryfieldMotelRoom2Task(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_2_H
