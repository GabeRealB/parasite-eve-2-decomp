#ifndef INCLUDE_ROOMS_DRYFIELD_PARKING_LOT_H
#define INCLUDE_ROOMS_DRYFIELD_PARKING_LOT_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_parking_lot_8017FA74[13];

// dryfield_parking_lot
extern GpRoomObjRec D_dryfield_parking_lot_8017DC44[];

extern u8* D_dryfield_parking_lot_8017DC54[];

extern ViewCount D_dryfield_parking_lot_8017DC58[];

extern WorldCoordRoomLighting D_dryfield_parking_lot_8017DC5C[];

extern GpWarpRec D_dryfield_parking_lot_8017DC64[];

extern ViewCamera D_dryfield_parking_lot_8017E900[];

extern SpriteView D_dryfield_parking_lot_8017F054[];

extern WorldCollisionSurfaceProperties* D_dryfield_parking_lot_8017FB30[];

void func_dryfield_parking_lot_8017DBAC(Task* unused);

void func_dryfield_parking_lot_8017DB54(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_PARKING_LOT_H
