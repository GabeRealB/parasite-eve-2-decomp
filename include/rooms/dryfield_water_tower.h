#ifndef INCLUDE_ROOMS_DRYFIELD_WATER_TOWER_H
#define INCLUDE_ROOMS_DRYFIELD_WATER_TOWER_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern u16 D_dryfield_water_tower_801876A8;

extern u16 D_dryfield_water_tower_801876AA;

extern GpAreaVariant D_dryfield_water_tower_8018757C[13];

// dryfield_water_tower
extern WorldCollisionRoomResources D_dryfield_water_tower_801827CC[];

extern u8* D_dryfield_water_tower_801827DC[];

extern ViewCount D_dryfield_water_tower_801827E0[];

extern WorldCoordRoomLighting D_dryfield_water_tower_801827E4[];

extern GpWarpRec D_dryfield_water_tower_801827EC[];

extern ViewCamera D_dryfield_water_tower_801835E8[];

extern SpriteView D_dryfield_water_tower_80186560[];

extern WorldCollisionSurfaceProperties* D_dryfield_water_tower_80187608[];

void func_dryfield_water_tower_80180348(Task* unused);

void func_dryfield_water_tower_8017DDD8(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_WATER_TOWER_H
