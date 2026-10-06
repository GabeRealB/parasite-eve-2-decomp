#ifndef INCLUDE_ROOMS_DRYFIELD_PARKING_LOT_H
#define INCLUDE_ROOMS_DRYFIELD_PARKING_LOT_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_parking_lot_8017FA74[13];

// dryfield_parking_lot
extern WorldCollisionRoomResources D_dryfield_parking_lot_8017DC44[];

extern u8* D_dryfield_parking_lot_8017DC54[];

extern ViewCount D_dryfield_parking_lot_8017DC58[];

extern WorldCoordRoomLighting D_dryfield_parking_lot_8017DC5C[];

extern DirectionWarpEntry D_dryfield_parking_lot_8017DC64[];

extern ViewCamera D_dryfield_parking_lot_8017E900[];

extern SpriteView D_dryfield_parking_lot_8017F054[];

extern WorldCollisionSurfaceProperties* D_dryfield_parking_lot_8017FB30[];

/// Updates the daytime parking lot's view gate for dust and related effects.
///
/// Enables the gate in mapped camera views 2 through 5 and disables it in
/// views 1, 6 and 7. The callback ignores `task` and runs without advancing or
/// killing it. Requires a live `gRoomEffectState`, the daytime parking lot
/// overlay and view tables loaded, and a mapped view index in 1 through 7.
void dryfieldParkingLotUpdateViewEffectGateTask(Task* task);

void func_dryfield_parking_lot_8017DB54(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_PARKING_LOT_H
