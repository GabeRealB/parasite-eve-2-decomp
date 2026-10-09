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

extern SpriteView D_dryfield_r04_8017E280[];

extern WorldCollisionSurfaceProperties* D_dryfield_r04_8017E2AC[];

#endif // INCLUDE_ROOMS_DRYFIELD_R04_H
