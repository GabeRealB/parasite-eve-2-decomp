#ifndef INCLUDE_ROOMS_NEO_ARK_FOREST_ZONE_H
#define INCLUDE_ROOMS_NEO_ARK_FOREST_ZONE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_neo_ark_forest_zone_80182968[13];

// neo_ark_forest_zone
extern WorldCollisionRoomResources D_neo_ark_forest_zone_801820A4[];

extern WorldCoordRoomLighting D_neo_ark_forest_zone_801820B4[];

extern u8* D_neo_ark_forest_zone_801820BC[];

extern ViewCount D_neo_ark_forest_zone_801820C0[];

extern DirectionWarpEntry D_neo_ark_forest_zone_801820C4[];

extern ViewCamera D_neo_ark_forest_zone_80182298[];

extern SpriteView D_neo_ark_forest_zone_80182594[];

extern WorldCollisionSurfaceProperties* D_neo_ark_forest_zone_80182CE4[];

void func_neo_ark_forest_zone_8017E3C0(Task* arg0);

void func_neo_ark_forest_zone_8017E420(Task* task);

void func_neo_ark_forest_zone_8017EE84(Task* task);

void func_neo_ark_forest_zone_8017F76C(Task* task);

void func_neo_ark_forest_zone_8017DC20(Task* task);

void func_neo_ark_forest_zone_8017DBBC(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_FOREST_ZONE_H
