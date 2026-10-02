#ifndef INCLUDE_ROOMS_DRYFIELD_WATER_TANK_H
#define INCLUDE_ROOMS_DRYFIELD_WATER_TANK_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_water_tank_80188BF0[13];

// dryfield_water_tank
extern WorldCollisionRoomResources D_dryfield_water_tank_801868E0[];

extern u8* D_dryfield_water_tank_801868F0[];

extern ViewCount D_dryfield_water_tank_801868F4[];

extern WorldCoordRoomLighting D_dryfield_water_tank_801868F8[];

extern DirectionWarpEntry D_dryfield_water_tank_80186900[];

extern ViewCamera D_dryfield_water_tank_80186EE0[];

extern SpriteView D_dryfield_water_tank_80187F80[];

extern WorldCollisionSurfaceProperties* D_dryfield_water_tank_80188CFC[];

void func_dryfield_water_tank_8017F084(Task* unused);

void func_dryfield_water_tank_8017DAF0(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_WATER_TANK_H
