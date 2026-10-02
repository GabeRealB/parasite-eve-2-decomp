#ifndef INCLUDE_ROOMS_SHELTER_B1_STERILIZATION_ROOM_H
#define INCLUDE_ROOMS_SHELTER_B1_STERILIZATION_ROOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TmdSource gShelterB1SterilizationRoomAcropolisSanctuaryModel090F0;

extern AreaVariant D_shelter_b1_sterilization_room_8018C14C[11];

// shelter_b1_sterilization_room
extern WorldCoordRoomLighting D_shelter_b1_sterilization_room_80189354[];

extern WorldCollisionRoomResources D_shelter_b1_sterilization_room_8018936C[];

extern u8* D_shelter_b1_sterilization_room_801893CC[];

extern ViewCount D_shelter_b1_sterilization_room_801893D8[];

extern DirectionWarpEntry D_shelter_b1_sterilization_room_801893E0[];

extern ViewCamera D_shelter_b1_sterilization_room_80189E68[];

extern SpriteView D_shelter_b1_sterilization_room_8018B00C[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_sterilization_room_8018C314[];

void func_shelter_b1_sterilization_room_80180518(Task* task);

void func_shelter_b1_sterilization_room_8018188C(Task* task);

void func_shelter_b1_sterilization_room_801823D8(Task* task);

void func_shelter_b1_sterilization_room_8017EB2C(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_STERILIZATION_ROOM_H
