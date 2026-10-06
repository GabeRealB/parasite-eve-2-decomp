#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_WAREHOUSE_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_WAREHOUSE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_warehouse_8017FB98[12];

// dryfield_night_warehouse
extern WorldCoordRoomLighting D_dryfield_night_warehouse_8017E8E8[];

extern WorldCollisionRoomResources D_dryfield_night_warehouse_8017E900[];

extern u8* D_dryfield_night_warehouse_8017E930[];

extern ViewCount D_dryfield_night_warehouse_8017E93C[];

extern DirectionWarpEntry D_dryfield_night_warehouse_8017E944[];

extern ViewCamera D_dryfield_night_warehouse_8017EF2C[];

extern SpriteView D_dryfield_night_warehouse_8017F46C[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_warehouse_8017FC24[];

/// Draws the warehouse's pulsing prism and four circular light beams for the current view.
///
/// Bank-6 effect 0x105 requires a live coordinate body owned by `task` and this
/// room overlay loaded. Composes the coordinate before drawing; the drawers
/// borrow it and queue packets in the current frame's arena. The view matrices,
/// scratch stack, primitive arena and ordering table must be ready for drawing.
/// The room-local view ID must be valid for its tables and less than 31.
/// Views 2, 3, 6 and 9 draw the prism and beams 2..4; view 2 adds beam 1.
/// Views 4, 7 and 8 draw only beams 3 and 4. Other views queue no geometry.
void dryfieldNightWarehouseDrawGlowsTask(Task* task);

void func_dryfield_night_warehouse_8017D65C(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_WAREHOUSE_H
