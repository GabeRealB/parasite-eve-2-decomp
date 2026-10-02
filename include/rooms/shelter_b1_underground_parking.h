#ifndef INCLUDE_ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H
#define INCLUDE_ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H

#include "common.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    TaskDesc value;
    u8       retained[4];
} ShelterB1UndergroundParkingStorage71F0;
STATIC_ASSERT_SIZEOF(ShelterB1UndergroundParkingStorage71F0, 16);

extern GpAreaVariant D_shelter_b1_underground_parking_8018B5C4[22];

extern ShelterB1UndergroundParkingStorage71F0 D_shelter_b1_underground_parking_801871F0;

// shelter_b1_underground_parking
extern WorldCoordRoomLighting D_shelter_b1_underground_parking_801877B4[];

extern WorldCollisionRoomResources D_shelter_b1_underground_parking_801877F4[];

extern u8* D_shelter_b1_underground_parking_8018791C[];

extern ViewCount D_shelter_b1_underground_parking_8018793C[];

extern DirectionWarpEntry D_shelter_b1_underground_parking_8018794C[];

extern ViewCamera D_shelter_b1_underground_parking_80189778[];

extern SpriteView D_shelter_b1_underground_parking_8018AB9C[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_underground_parking_8018D724[];

void func_shelter_b1_underground_parking_801838B4(Task* task);

void func_shelter_b1_underground_parking_8017EDE8(Task* task);

void func_shelter_b1_underground_parking_80184A18(Task* unused);

#endif // INCLUDE_ROOMS_SHELTER_B1_UNDERGROUND_PARKING_H
