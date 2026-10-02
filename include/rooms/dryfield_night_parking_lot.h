#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_PARKING_LOT_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_PARKING_LOT_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
extern GpAreaApplyRec gParkingLotAreaRecs[2];

extern GpAreaVariant D_dryfield_night_parking_lot_80181438[22];

// dryfield_night_parking_lot
extern WorldCoordRoomLighting D_dryfield_night_parking_lot_8017EE14[];

extern WorldCollisionRoomResources D_dryfield_night_parking_lot_8017EE24[];

extern u8* D_dryfield_night_parking_lot_8017EE4C[];

extern ViewCount D_dryfield_night_parking_lot_8017EE54[];

extern GpWarpRec D_dryfield_night_parking_lot_8017EE58[];

extern ViewCamera D_dryfield_night_parking_lot_8017FAF4[];

extern SpriteView D_dryfield_night_parking_lot_801805AC[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_parking_lot_8018153C[];

void func_dryfield_night_parking_lot_8017DC88(Task* unused);

void func_dryfield_night_parking_lot_8017DC30(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_PARKING_LOT_H
