#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_DRIVEWAY_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_DRIVEWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_night_driveway_80181F4C[22];

// dryfield_night_driveway
extern WorldCoordRoomLighting D_dryfield_night_driveway_801805E0[];

extern GpRoomObjRec D_dryfield_night_driveway_801805F0[];

extern u8* D_dryfield_night_driveway_8018061C[];

extern ViewCount D_dryfield_night_driveway_80180624[];

extern GpWarpRec D_dryfield_night_driveway_80180628[];

extern ViewCamera D_dryfield_night_driveway_80180C30[];

extern SpriteView D_dryfield_night_driveway_80181870[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_driveway_801820F0[];

void func_dryfield_night_driveway_8017DD8C(Task* task);

void func_dryfield_night_driveway_8017E5CC(Task* unused);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_DRIVEWAY_H
