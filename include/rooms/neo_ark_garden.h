#ifndef INCLUDE_ROOMS_NEO_ARK_GARDEN_H
#define INCLUDE_ROOMS_NEO_ARK_GARDEN_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_neo_ark_garden_80181398;

extern GpAreaVariant D_neo_ark_garden_80182B54[13];

// neo_ark_garden
extern WorldCollisionRoomResources D_neo_ark_garden_8018140C[];

extern WorldCoordRoomLighting D_neo_ark_garden_8018141C[];

extern u8* D_neo_ark_garden_80181424[];

extern ViewCount D_neo_ark_garden_80181428[];

extern GpWarpRec D_neo_ark_garden_8018142C[];

extern ViewCamera D_neo_ark_garden_801816E8[];

extern SpriteView D_neo_ark_garden_80182540[];

extern WorldCollisionSurfaceProperties* D_neo_ark_garden_80182BD8[];

void func_neo_ark_garden_8017EA9C(Task* task);

void func_neo_ark_garden_8017FCE8(Task* task);

void func_neo_ark_garden_8017F790(Task* arg0);

void func_neo_ark_garden_80180948(Task* arg0);

void func_neo_ark_garden_8017EA44(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_GARDEN_H
