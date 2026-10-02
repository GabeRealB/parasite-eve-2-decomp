#ifndef INCLUDE_ROOMS_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_H
#define INCLUDE_ROOMS_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_shelter_b2_south_maintenance_walkway_801837AC[22];

// shelter_b2_south_maintenance_walkway
extern u8* D_shelter_b2_south_maintenance_walkway_8018263C[];

extern GpViewCountRec D_shelter_b2_south_maintenance_walkway_80182640[];

extern GpWarpRec D_shelter_b2_south_maintenance_walkway_80182644[];

extern WorldCollisionGrid D_shelter_b2_south_maintenance_walkway_801829E8;

extern ViewCamera D_shelter_b2_south_maintenance_walkway_80182A0C[];

extern SpriteView D_shelter_b2_south_maintenance_walkway_80183018[];

extern WorldCoordRoomLights D_shelter_b2_south_maintenance_walkway_80183294;

extern WorldCollisionTrigger D_shelter_b2_south_maintenance_walkway_801832AC[];

extern WorldCollisionTrigger D_shelter_b2_south_maintenance_walkway_80183474[];

extern WorldCollisionOccluder D_shelter_b2_south_maintenance_walkway_8018385C[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_south_maintenance_walkway_801838B4[];

void func_shelter_b2_south_maintenance_walkway_8017DC6C(Task* task);

void func_shelter_b2_south_maintenance_walkway_8017DCC4(Task* task);

void func_shelter_b2_south_maintenance_walkway_8017E99C(Task* task);

void func_shelter_b2_south_maintenance_walkway_8017F400(Task* task);

void func_shelter_b2_south_maintenance_walkway_8017FCE8(Task* task);

void func_shelter_b2_south_maintenance_walkway_80180930(Task* arg0);

void func_shelter_b2_south_maintenance_walkway_80180E88(Task* task);

void func_shelter_b2_south_maintenance_walkway_80181AE8(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_H
