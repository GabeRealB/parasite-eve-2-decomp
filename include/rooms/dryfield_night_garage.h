#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_GARAGE_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_GARAGE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_dryfield_night_garage_80183380[2];

extern TaskDesc D_dryfield_night_garage_80181C2C;

extern GpAreaVariant D_dryfield_night_garage_801874BC[12];

// dryfield_night_garage
extern WorldCoordRoomLighting D_dryfield_night_garage_801833F4[];

extern GpRoomObjRec D_dryfield_night_garage_80183404[];

extern u8* D_dryfield_night_garage_80183434[];

extern GpViewCountRec D_dryfield_night_garage_8018343C[];

extern GpWarpRec D_dryfield_night_garage_80183440[];

extern GpViewRec D_dryfield_night_garage_801843F8[];

extern SpriteView D_dryfield_night_garage_80186258[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_garage_801875B8[];

void func_dryfield_night_garage_80180414(s32 arg0);

void func_dryfield_night_garage_80181518(Task* unused);

void func_dryfield_night_garage_801803BC(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_GARAGE_H
