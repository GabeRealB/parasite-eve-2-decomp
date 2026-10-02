#ifndef INCLUDE_ROOMS_NEO_ARK_POWER_PLANT_1_H
#define INCLUDE_ROOMS_NEO_ARK_POWER_PLANT_1_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_neo_ark_power_plant_1_80181AF8[13];

// neo_ark_power_plant_1
extern GpRoomObjRec D_neo_ark_power_plant_1_8017F1C8[];

extern WorldCoordRoomLighting D_neo_ark_power_plant_1_8017F1D8[];

extern u8* D_neo_ark_power_plant_1_8017F1E0[];

extern GpViewCountRec D_neo_ark_power_plant_1_8017F1E4[];

extern GpWarpRec D_neo_ark_power_plant_1_8017F1E8[];

extern ViewCamera D_neo_ark_power_plant_1_801800B4[];

extern SpriteView D_neo_ark_power_plant_1_801814F0[];

extern WorldCollisionSurfaceProperties* D_neo_ark_power_plant_1_80181BE0[];

void func_neo_ark_power_plant_1_8017E524(s32 arg0);

void func_neo_ark_power_plant_1_8017DA18(Task* unused);

void func_neo_ark_power_plant_1_8017D9C0(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_POWER_PLANT_1_H
