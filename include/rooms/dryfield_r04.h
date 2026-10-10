#ifndef INCLUDE_ROOMS_DRYFIELD_R04_H
#define INCLUDE_ROOMS_DRYFIELD_R04_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

/// Collision resources for the area's single room, indexed by room minus one.
///
/// Borrows the loaded room's 9-by-9 collision grid; no view boundaries, action
/// triggers or sight occluders are supplied. Keep the overlay loaded while used.
extern WorldCollisionRoomResources gDryfieldR04RoomCollisionResources[1];

/// Logical-view map directory for the area's single room selector.
///
/// Entry zero borrows `gViewIdentityMap`: logical views 1..2 select the same
/// camera, image and sprite indices. Room IDs index at room minus one; there
/// is no terminator. The directory requires this room overlay to stay loaded.
extern u8* gDryfieldR04ViewMaps[1];

/// Two logical views for the area's single room, indexed by room minus one.
///
/// Reverse lookup searches the first two identity-map entries. The stage map
/// borrows this signed-halfword count while this room overlay is loaded.
extern ViewCount gDryfieldR04RoomViewCounts[1];

/// The area's sole warp endpoint, selecting logical view 2 on arrival.
///
/// Player and companion spawn transforms are zero. Sounds, departure flags and
/// map-marker updates are disabled. The stage map borrows this complete entry
/// while the room overlay is loaded; warp IDs index at warp minus one.
extern DirectionWarpEntry gDryfieldR04WarpEntries[1];

/// World-to-camera transforms for mapped views 1 and 2, indexed by view minus one.
///
/// Both use world +Y as camera depth and a 207-pixel projection distance.
/// The stage map borrows these records while the room overlay is loaded.
extern ViewCamera gDryfieldR04ViewCameras[2];

/// Dryfield r04's sprite resources, indexed by mapped view minus one.
///
/// Both views retain decoded backgrounds without source sprites.
/// Their distinct empty batch-list addresses also occupy sources.empty.
/// Every slot retains decoded image strips and has no clipping list. Empty
/// source slots retain batch addresses without supplying SpriteSource elements.
/// Descriptors and mutable source/batch lists stay borrowed while the owning
/// room overlay is loaded; selectors must be nonzero and within this array.
extern SpriteView gDryfieldR04SpriteViews[2];

/// Dryfield R04 collision and footstep policies for surface classes 0..7.
///
/// All eight classes share one record: probes block, weapon impacts are enabled,
/// pushback applies and footsteps are silent. Setup reads every entry; borrow
/// the directory and record only while this room overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldR04SurfacePropertiesByClass[8];

#endif // INCLUDE_ROOMS_DRYFIELD_R04_H
