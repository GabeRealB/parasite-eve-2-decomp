#ifndef INCLUDE_ROOMS_SHELTER_B6_CORRIDOR_H
#define INCLUDE_ROOMS_SHELTER_B6_CORRIDOR_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b6_corridor_80180304[13];

// shelter_b6_corridor
extern u8* D_shelter_b6_corridor_8017F8B4[];

extern ViewCount D_shelter_b6_corridor_8017F8B8[];

extern DirectionWarpEntry D_shelter_b6_corridor_8017F8BC[];

extern WorldCollisionGrid D_shelter_b6_corridor_8017FA90;

extern ViewCamera D_shelter_b6_corridor_8017FAB4[];

extern SpriteView D_shelter_b6_corridor_8018004C[];

extern WorldCoordRoomLights D_shelter_b6_corridor_801800E8;

extern WorldCollisionTrigger D_shelter_b6_corridor_80180100[];

extern WorldCollisionTrigger D_shelter_b6_corridor_8018036C[];

extern WorldCoordRoomAmbientEntry D_shelter_b6_corridor_801804E8[];

extern WorldCollisionSurfaceProperties* D_shelter_b6_corridor_80180548[];

void func_shelter_b6_corridor_8017E144(Task* task);

void func_shelter_b6_corridor_8017EBA4(Task* task);

void func_shelter_b6_corridor_8017EE08(s32 arg0, s32 arg1);

void func_shelter_b6_corridor_8017E238(Task* task);

void func_shelter_b6_corridor_8017ECA8(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B6_CORRIDOR_H
