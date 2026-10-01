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

extern GpViewCountRec D_shelter_b2_elevator_hall_801838E0[];

extern GpWarpRec D_shelter_b2_elevator_hall_801838E4[];

extern GpGridParams D_shelter_b2_elevator_hall_80183DB4;

extern GpViewRec D_shelter_b2_elevator_hall_80183DD8[];

extern GpSprtRec D_shelter_b2_elevator_hall_80184120[];

extern GpRoomCoordSet D_shelter_b2_elevator_hall_801846B4;

extern GpObj4A D_shelter_b2_elevator_hall_801846CC[];

extern GpObj3A D_shelter_b2_elevator_hall_8018492C[];

extern GpObj4A D_shelter_b2_elevator_hall_80184968[];

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
