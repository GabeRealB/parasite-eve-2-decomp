#ifndef INCLUDE_ROOMS_SHELTER_B1_STOREROOM_H
#define INCLUDE_ROOMS_SHELTER_B1_STOREROOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_shelter_b1_storeroom_80186C9C[22];

// shelter_b1_storeroom
extern WorldCollisionRoomResources D_shelter_b1_storeroom_80184B50[];

extern WorldCoordRoomLighting D_shelter_b1_storeroom_80184B60[];

extern u8* D_shelter_b1_storeroom_80184B68[];

extern ViewCount D_shelter_b1_storeroom_80184B6C[];

extern GpWarpRec D_shelter_b1_storeroom_80184B70[];

extern ViewCamera D_shelter_b1_storeroom_801850FC[];

extern SpriteView D_shelter_b1_storeroom_80186090[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_storeroom_80186DEC[];

void func_shelter_b1_storeroom_8017D794(Task* task);

void func_shelter_b1_storeroom_80180DCC(Task* arg0);

void func_shelter_b1_storeroom_80181830(Task* task);

void func_shelter_b1_storeroom_80182118(Task* task);

void func_shelter_b1_storeroom_80182D60(Task* arg0);

void func_shelter_b1_storeroom_801832B8(Task* task);

void func_shelter_b1_storeroom_80183F18(Task* task);

void func_shelter_b1_storeroom_8017E7A8(Task* task);

void func_shelter_b1_storeroom_8017F4F0(Task* arg0);

void func_shelter_b1_storeroom_8017F888(Task* arg0);

void func_shelter_b1_storeroom_80180C98(Task* arg0);

void func_shelter_b1_storeroom_8017D7EC(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B1_STOREROOM_H
