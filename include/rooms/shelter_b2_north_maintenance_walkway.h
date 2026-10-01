#ifndef INCLUDE_ROOMS_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_H
#define INCLUDE_ROOMS_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_shelter_b2_north_maintenance_walkway_80186258[22];

// shelter_b2_north_maintenance_walkway
extern u8* D_shelter_b2_north_maintenance_walkway_80183C5C[];

extern GpViewCountRec D_shelter_b2_north_maintenance_walkway_80183C60[];

extern GpWarpRec D_shelter_b2_north_maintenance_walkway_80183C64[];

extern WorldCollisionGrid D_shelter_b2_north_maintenance_walkway_8018401C;

extern GpViewRec D_shelter_b2_north_maintenance_walkway_80184040[];

extern SpriteView D_shelter_b2_north_maintenance_walkway_80185B04[];

extern WorldCoordRoomLights D_shelter_b2_north_maintenance_walkway_80185D44;

extern WorldCollisionTrigger D_shelter_b2_north_maintenance_walkway_80185D5C[];

extern WorldCollisionTrigger D_shelter_b2_north_maintenance_walkway_80185F24[];

extern WorldCollisionOccluder D_shelter_b2_north_maintenance_walkway_80186308[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_north_maintenance_walkway_80186360[];

void func_shelter_b2_north_maintenance_walkway_8017DD90(Task* task);

void func_shelter_b2_north_maintenance_walkway_80181BB4(Task* arg0);

void func_shelter_b2_north_maintenance_walkway_80182618(Task* task);

void func_shelter_b2_north_maintenance_walkway_80182F00(Task* task);

void func_shelter_b2_north_maintenance_walkway_8017F590(Task* task);

void func_shelter_b2_north_maintenance_walkway_801802D8(Task* arg0);

void func_shelter_b2_north_maintenance_walkway_80180670(Task* arg0);

void func_shelter_b2_north_maintenance_walkway_80181A80(Task* arg0);

void func_shelter_b2_north_maintenance_walkway_8017DDE8(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_H
