#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_TANK_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_TANK_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_water_tank_80180764[13];

// dryfield_night_water_tank
extern WorldCoordRoomLighting D_dryfield_night_water_tank_8017EE50[];

extern WorldCollisionRoomResources D_dryfield_night_water_tank_8017EE58[];

extern u8* D_dryfield_night_water_tank_8017EE74[];

extern ViewCount D_dryfield_night_water_tank_8017EE78[];

extern DirectionWarpEntry D_dryfield_night_water_tank_8017EE7C[];

extern ViewCamera D_dryfield_night_water_tank_8017F4D4[];

extern SpriteView D_dryfield_night_water_tank_801801CC[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_water_tank_80180890[];

void func_dryfield_night_water_tank_8017DD8C(Task* unused);

void func_dryfield_night_water_tank_8017D984(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_WATER_TANK_H
