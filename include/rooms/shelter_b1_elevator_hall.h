#ifndef INCLUDE_ROOMS_SHELTER_B1_ELEVATOR_HALL_H
#define INCLUDE_ROOMS_SHELTER_B1_ELEVATOR_HALL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_shelter_b1_elevator_hall_80184940[12];

// shelter_b1_elevator_hall
extern WorldCoordRoomLighting D_shelter_b1_elevator_hall_80182DF8[];

extern WorldCollisionRoomResources D_shelter_b1_elevator_hall_80182E00[];

extern u8* D_shelter_b1_elevator_hall_80182E10[];

extern ViewCount D_shelter_b1_elevator_hall_80182E14[];

extern GpWarpRec D_shelter_b1_elevator_hall_80182E18[];

extern ViewCamera D_shelter_b1_elevator_hall_80183438[];

extern SpriteView D_shelter_b1_elevator_hall_80183CC4[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_elevator_hall_801849D0[];

void func_shelter_b1_elevator_hall_8017DC28(Task* task);

void func_shelter_b1_elevator_hall_80180D18(Task* arg0);

void func_shelter_b1_elevator_hall_8018177C(Task* task);

void func_shelter_b1_elevator_hall_8017E6F4(Task* task);

void func_shelter_b1_elevator_hall_8017F43C(Task* arg0);

void func_shelter_b1_elevator_hall_80180BE4(Task* arg0);

void func_shelter_b1_elevator_hall_8017DC80(Task* arg0);

void func_shelter_b1_elevator_hall_80182064(Task* task);

void func_shelter_b1_elevator_hall_8017F7D4(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B1_ELEVATOR_HALL_H
