#ifndef INCLUDE_ROOMS_ACROPOLIS_WEST_ELEVATOR_HALL_H
#define INCLUDE_ROOMS_ACROPOLIS_WEST_ELEVATOR_HALL_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// acropolis_west_elevator_hall
extern WorldCollisionRoomResources D_acropolis_west_elevator_hall_80185024[];

extern u8* D_acropolis_west_elevator_hall_80185034[];

extern ViewCount D_acropolis_west_elevator_hall_80185038[];

extern WorldCoordRoomLighting D_acropolis_west_elevator_hall_8018503C[];

extern DirectionWarpEntry D_acropolis_west_elevator_hall_80185044[];

extern SpriteView D_acropolis_west_elevator_hall_80186408[];

extern ViewCamera D_acropolis_west_elevator_hall_801869FC[];

extern WorldCollisionSurfaceProperties* D_acropolis_west_elevator_hall_80186AC4[];

void acropolisWestElevatorHallRedBeaconTask(Task* arg0);

void func_acropolis_west_elevator_hall_8017FE18(Task* task);

void func_acropolis_west_elevator_hall_8017FFE4(Task* arg0);

void func_acropolis_west_elevator_hall_8017F990(Task* task);

void func_acropolis_west_elevator_hall_8017F304(Task* task);

void func_acropolis_west_elevator_hall_8017F7D4(Task* task);

void func_acropolis_west_elevator_hall_8017F5F4(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_WEST_ELEVATOR_HALL_H
