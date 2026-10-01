#ifndef INCLUDE_ROOMS_SHELTER_B4_WATER_SUPPLY_H
#define INCLUDE_ROOMS_SHELTER_B4_WATER_SUPPLY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern u8 D_shelter_b4_water_supply_801826FC[64];

extern GpAreaVariant D_shelter_b4_water_supply_80184CA4[12];

// shelter_b4_water_supply
extern u8* D_shelter_b4_water_supply_8018273C[];

extern GpViewCountRec D_shelter_b4_water_supply_80182740[];

extern GpWarpRec D_shelter_b4_water_supply_80182744[];

extern WorldCollisionGrid D_shelter_b4_water_supply_80182E3C;

extern GpViewRec D_shelter_b4_water_supply_80182E60[];

extern SpriteView D_shelter_b4_water_supply_80183F90[];

extern WorldCoordRoomLights D_shelter_b4_water_supply_801843D4;

extern WorldCollisionTrigger D_shelter_b4_water_supply_801843EC[];

extern WorldCollisionTrigger D_shelter_b4_water_supply_80184944[];

extern WorldCollisionOccluder D_shelter_b4_water_supply_80184D04[];

extern WorldCoordRoomAmbientEntry D_shelter_b4_water_supply_80184D7C[];

extern WorldCollisionSurfaceProperties* D_shelter_b4_water_supply_80184E14[];

void func_shelter_b4_water_supply_8017DDA4(Task* task);

void func_shelter_b4_water_supply_8017EE54(Task* arg0);

void func_shelter_b4_water_supply_8017F24C(Task* task);

void func_shelter_b4_water_supply_8017F6D4(Task* task);

void func_shelter_b4_water_supply_801809DC(Task* arg0);

void func_shelter_b4_water_supply_80180F34(Task* task);

void func_shelter_b4_water_supply_80181B94(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B4_WATER_SUPPLY_H
