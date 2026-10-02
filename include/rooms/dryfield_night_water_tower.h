#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_TOWER_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_TOWER_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_night_water_tower_80182B5C[22];

// dryfield_night_water_tower
extern WorldCoordRoomLighting D_dryfield_night_water_tower_8017E74C[];

extern GpRoomObjRec D_dryfield_night_water_tower_8017E754[];

extern u8* D_dryfield_night_water_tower_8017E764[];

extern ViewCount D_dryfield_night_water_tower_8017E768[];

extern GpWarpRec D_dryfield_night_water_tower_8017E76C[];

extern ViewCamera D_dryfield_night_water_tower_8017F418[];

extern SpriteView D_dryfield_night_water_tower_80182040[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_water_tower_80182C30[];

void func_dryfield_night_water_tower_8017DB80(Task* unused);

void func_dryfield_night_water_tower_8017DB28(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_TOWER_H
