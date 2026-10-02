#ifndef INCLUDE_ROOMS_SHELTER_B3_ELEVATOR_HALL_H
#define INCLUDE_ROOMS_SHELTER_B3_ELEVATOR_HALL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_shelter_b3_elevator_hall_8018477C[12];

// shelter_b3_elevator_hall
extern u8* D_shelter_b3_elevator_hall_80182B54[];

extern ViewCount D_shelter_b3_elevator_hall_80182B58[];

extern DirectionWarpEntry D_shelter_b3_elevator_hall_80182B5C[];

extern WorldCollisionGrid D_shelter_b3_elevator_hall_801834C8;

extern ViewCamera D_shelter_b3_elevator_hall_801834EC[];

extern SpriteView D_shelter_b3_elevator_hall_801841DC[];

extern WorldCoordRoomLights D_shelter_b3_elevator_hall_80184410;

extern WorldCollisionTrigger D_shelter_b3_elevator_hall_80184428[];

extern WorldCollisionTrigger D_shelter_b3_elevator_hall_801847DC[];

extern WorldCollisionOccluder D_shelter_b3_elevator_hall_8018490C[];

extern WorldCollisionSurfaceProperties* D_shelter_b3_elevator_hall_801849E0[];

void func_shelter_b3_elevator_hall_8017DE18(Task* task);

void func_shelter_b3_elevator_hall_8017DE70(Task* arg0);

void func_shelter_b3_elevator_hall_80180E18(Task* arg0);

void func_shelter_b3_elevator_hall_80181370(Task* task);

void func_shelter_b3_elevator_hall_80181FD0(Task* arg0);

void func_shelter_b3_elevator_hall_8017E7F4(Task* task);

void func_shelter_b3_elevator_hall_8017F53C(Task* arg0);

void func_shelter_b3_elevator_hall_8017F8D4(Task* arg0);

void func_shelter_b3_elevator_hall_80180CE4(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B3_ELEVATOR_HALL_H
