#ifndef INCLUDE_ROOMS_DRYFIELD_MOTEL_LOFT_H
#define INCLUDE_ROOMS_DRYFIELD_MOTEL_LOFT_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Collision resources for the motel loft's single room, indexed by room ID minus one.
///
/// Borrows the overlay's writable grid, twelve view boundaries and two action
/// triggers; there are no sight occluders. Keep the overlay loaded while the
/// resources are active and unlink the triggers before unloading it.
extern WorldCollisionRoomResources gDryfieldMotelLoftRoomCollisionResources[1];

/// Logical-view map directory for the motel loft's single room.
///
/// Room 1 borrows gameplay's identity map: logical views 1..7 select matching
/// camera, image and sprite slots. Index the directory by room ID minus one
/// and the borrowed byte map by logical view minus one. Both overlays must stay loaded.
extern u8* gDryfieldMotelLoftRoomViewMaps[1];

/// Seven logical views for room 1, limiting reverse searches of its identity map.
///
/// Indexed by room ID minus one; the loft overlay owns the signed element count.
extern ViewCount gDryfieldMotelLoftRoomViewCounts[1];

/// Lighting for the motel loft's single room, indexed by room ID minus one.
///
/// Borrows ten mutable point lights contributing in every view and uses the
/// default ambient minima. Keep the loft overlay loaded while lighting uses it.
extern WorldCoordRoomLighting gDryfieldMotelLoftRoomLighting[1];

/// The motel loft's sole warp endpoint, connecting it to the motel balcony.
///
/// Endpoint 1 places both actors at (3953, 0, -2156), with yaw zero and view 2.
/// Supplies arrival/departure sounds and map marker 466; blocked departures are
/// silent. Borrowed by the stage directory while this overlay is loaded.
extern DirectionWarpEntry gDryfieldMotelLoftWarpEntries[1];

/// Seven cameras selected by the motel loft's mapped view index minus one.
///
/// Rotations use 4096 per unit, translations are negated camera origins in
/// world units, and projection distances are pixels. Keep the overlay loaded
/// until any queued camera application has copied its record.
extern ViewCamera gDryfieldMotelLoftViewCameras[7];

/// Sprite resources for mapped motel loft views 1..7, indexed by view minus one.
///
/// Every view retains the decoded image background; views 4..7 add respectively
/// nine, eight, seven and seven depth-sorted sprites. No view supplies clipping
/// areas. Records and writable backing arrays remain borrowed while the loft is loaded.
extern SpriteView gDryfieldMotelLoftSpriteViews[7];

/// Collision policies for all eight motel loft surface classes, indexed by class 0..7.
///
/// Class 1 has a distinct floor record; all other slots share the default record.
/// Both block probes, permit weapon impacts and apply pushback, with no footstep
/// cues. The records are borrowed only while the loft overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldMotelLoftSurfacePropertiesByClass[8];

/// Runs the motel loft's room-message receiver.
///
/// Start the borrowed task in state 0 to install this overlay's message table
/// and register `GAME_TASK_SLOT_ROOM`; state 1 idles and state 2 releases it.
/// The state index must be 0..2; dispatch is unchecked. Keep this overlay and
/// the registered task live while messages can arrive. Registration borrows
/// the task pointer and does not clear the slot on teardown.
void dryfieldMotelLoftTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MOTEL_LOFT_H
