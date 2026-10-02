#ifndef INCLUDE_ROOMS_SHELTER_B2_ELEVATOR_HALL_H
#define INCLUDE_ROOMS_SHELTER_B2_ELEVATOR_HALL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_shelter_b2_elevator_hall_80184C7C[22];

// shelter_b2_elevator_hall
extern u8* D_shelter_b2_elevator_hall_801838DC[];

extern ViewCount D_shelter_b2_elevator_hall_801838E0[];

extern DirectionWarpEntry D_shelter_b2_elevator_hall_801838E4[];

extern WorldCollisionGrid D_shelter_b2_elevator_hall_80183DB4;

extern ViewCamera D_shelter_b2_elevator_hall_80183DD8[];

extern SpriteView D_shelter_b2_elevator_hall_80184120[];

extern WorldCoordRoomLights D_shelter_b2_elevator_hall_801846B4;

extern WorldCollisionTrigger D_shelter_b2_elevator_hall_801846CC[];

extern WorldCollisionOccluder D_shelter_b2_elevator_hall_8018492C[];

extern WorldCollisionTrigger D_shelter_b2_elevator_hall_80184968[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_elevator_hall_80184D5C[];

void func_shelter_b2_elevator_hall_8017DD08(Task* task);

void func_shelter_b2_elevator_hall_801817FC(Task* arg0);

void func_shelter_b2_elevator_hall_80182260(Task* task);

void func_shelter_b2_elevator_hall_80182B48(Task* task);

void func_shelter_b2_elevator_hall_8017F1D8(Task* task);

void func_shelter_b2_elevator_hall_8017FF20(Task* arg0);

void func_shelter_b2_elevator_hall_801802B8(Task* arg0);

void func_shelter_b2_elevator_hall_801816C8(Task* arg0);

void func_shelter_b2_elevator_hall_8017DD60(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B2_ELEVATOR_HALL_H
