#ifndef INCLUDE_ROOMS_ACROPOLIS_EAST_ELEVATOR_HALL_H
#define INCLUDE_ROOMS_ACROPOLIS_EAST_ELEVATOR_HALL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_acropolis_east_elevator_hall_80186C50[3];

// acropolis_east_elevator_hall
extern WorldCollisionRoomResources D_acropolis_east_elevator_hall_80186320[];

extern u8* D_acropolis_east_elevator_hall_80186330[];

extern ViewCount D_acropolis_east_elevator_hall_80186334[];

extern WorldCoordRoomLighting D_acropolis_east_elevator_hall_80186338[];

extern DirectionWarpEntry D_acropolis_east_elevator_hall_80186340[];

extern SpriteView D_acropolis_east_elevator_hall_80187870[];

extern ViewCamera D_acropolis_east_elevator_hall_80187A5C[];

extern WorldCollisionSurfaceProperties* D_acropolis_east_elevator_hall_80187B74[];

void func_acropolis_east_elevator_hall_8017F5B4(Task* task);

void acropolisEastElevatorHallRedBeaconTask(Task* arg0);

void func_acropolis_east_elevator_hall_8017F2F8(Task* task);

void func_acropolis_east_elevator_hall_8017F55C(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_EAST_ELEVATOR_HALL_H
