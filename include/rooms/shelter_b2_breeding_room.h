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

extern TmdSource D_shelter_b2_breeding_room_801803F0;

// shelter_b2_breeding_room
extern u8* D_shelter_b2_breeding_room_8018055C[];

extern GpViewCountRec D_shelter_b2_breeding_room_80180560[];

extern GpWarpRec D_shelter_b2_breeding_room_80180564[];

extern GpGridParams D_shelter_b2_breeding_room_801810F4;

extern GpViewRec D_shelter_b2_breeding_room_80181118[];

extern GpSprtRec D_shelter_b2_breeding_room_801833D4[];

extern GpRoomCoordSet D_shelter_b2_breeding_room_801837AC;

extern GpObj4A D_shelter_b2_breeding_room_801837C4[];

extern GpObj4A D_shelter_b2_breeding_room_80183F9C[];

extern GpRoomBoundVec D_shelter_b2_breeding_room_80184624[];

extern GpObj3A D_shelter_b2_breeding_room_8018467C[];

extern GpRoomParamRec* D_shelter_b2_breeding_room_801847F4[];

extern GpAreaVariant D_shelter_b2_breeding_room_80183EEC[22];

void func_shelter_b2_breeding_room_8017D5F8(Task* task);

void func_shelter_b2_breeding_room_8017D840(Task* task);

void func_shelter_b2_breeding_room_8017E774(Task* arg0);

void func_shelter_b2_breeding_room_8017ECCC(Task* task);

void func_shelter_b2_breeding_room_8017F92C(Task* arg0);

void func_shelter_b2_breeding_room_8017D898(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B2_BREEDING_ROOM_H
