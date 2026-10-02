#ifndef INCLUDE_ROOMS_SHELTER_B2_BREEDING_ROOM_H
#define INCLUDE_ROOMS_SHELTER_B2_BREEDING_ROOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TmdSource gShelterB2BreedingRoomModel02E04;

// shelter_b2_breeding_room
extern u8* D_shelter_b2_breeding_room_8018055C[];

extern ViewCount D_shelter_b2_breeding_room_80180560[];

extern DirectionWarpEntry D_shelter_b2_breeding_room_80180564[];

extern WorldCollisionGrid D_shelter_b2_breeding_room_801810F4;

extern ViewCamera D_shelter_b2_breeding_room_80181118[];

extern SpriteView D_shelter_b2_breeding_room_801833D4[];

extern WorldCoordRoomLights D_shelter_b2_breeding_room_801837AC;

extern WorldCollisionTrigger D_shelter_b2_breeding_room_801837C4[];

extern WorldCollisionTrigger D_shelter_b2_breeding_room_80183F9C[];

extern WorldCoordRoomAmbientEntry D_shelter_b2_breeding_room_80184624[];

extern WorldCollisionOccluder D_shelter_b2_breeding_room_8018467C[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_breeding_room_801847F4[];

extern GpAreaVariant D_shelter_b2_breeding_room_80183EEC[22];

void func_shelter_b2_breeding_room_8017D5F8(Task* task);

void func_shelter_b2_breeding_room_8017D840(Task* task);

void func_shelter_b2_breeding_room_8017E774(Task* arg0);

void func_shelter_b2_breeding_room_8017ECCC(Task* task);

void func_shelter_b2_breeding_room_8017F92C(Task* arg0);

void func_shelter_b2_breeding_room_8017D898(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B2_BREEDING_ROOM_H
