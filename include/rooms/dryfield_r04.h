#ifndef INCLUDE_ROOMS_DRYFIELD_R04_H
#define INCLUDE_ROOMS_DRYFIELD_R04_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

// dryfield_r04
extern WorldCollisionRoomResources D_dryfield_r04_8017D5C4[];

/// Logical-view map directory for the area's single room selector.
///
/// Entry zero borrows `gViewIdentityMap`: logical views 1..2 select the same
/// camera, image and sprite indices. Room IDs index at room minus one; there
/// is no terminator. The directory requires this room overlay to stay loaded.
extern u8* gDryfieldR04ViewMaps[1];

extern ViewCount D_dryfield_r04_8017D5D8[];

extern DirectionWarpEntry D_dryfield_r04_8017D5E4[];

extern ViewCamera D_dryfield_r04_8017E218[];

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
