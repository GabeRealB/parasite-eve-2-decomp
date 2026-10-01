#ifndef INCLUDE_ROOMS_SHELTER_B6_NURSERY_H
#define INCLUDE_ROOMS_SHELTER_B6_NURSERY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TmdSource D_shelter_b6_nursery_801852D0;

extern GpAreaVariant D_shelter_b6_nursery_801874A4[13];

// shelter_b6_nursery
extern u8* D_shelter_b6_nursery_80185304[];

extern GpViewCountRec D_shelter_b6_nursery_80185308[];

extern GpWarpRec D_shelter_b6_nursery_8018530C[];

extern WorldCollisionGrid D_shelter_b6_nursery_801858A0;

extern GpViewRec D_shelter_b6_nursery_801858C4[];

extern SpriteView D_shelter_b6_nursery_80186FD0[];

extern WorldCoordRoomLights D_shelter_b6_nursery_80187294;

extern WorldCollisionTrigger D_shelter_b6_nursery_801872AC[];

extern WorldCollisionTrigger D_shelter_b6_nursery_8018750C[];

extern WorldCoordRoomAmbientEntry D_shelter_b6_nursery_8018789C[];

extern WorldCollisionSurfaceProperties* D_shelter_b6_nursery_80187958[];

void func_shelter_b6_nursery_8017FF9C(Task* task);

void func_shelter_b6_nursery_8017FFF4(void);

void func_shelter_b6_nursery_80180038(s32 arg0);

void func_shelter_b6_nursery_80182730(Task* task);

void func_shelter_b6_nursery_80182D14(s32 arg0, s32 arg1);

void func_shelter_b6_nursery_801800A0(Task* task);

void func_shelter_b6_nursery_80181314(Task* task);

void func_shelter_b6_nursery_80181820(Task* task);

void func_shelter_b6_nursery_80182D28(Task* task);

void func_shelter_b6_nursery_8018378C(Task* task);

void func_shelter_b6_nursery_80184074(Task* task);

void func_shelter_b6_nursery_8017EAC4(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B6_NURSERY_H
