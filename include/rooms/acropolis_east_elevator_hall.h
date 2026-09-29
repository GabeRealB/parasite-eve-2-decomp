#ifndef INCLUDE_ROOMS_ACROPOLIS_EAST_ELEVATOR_HALL_H
#define INCLUDE_ROOMS_ACROPOLIS_EAST_ELEVATOR_HALL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_acropolis_east_elevator_hall_80186C50[3];

// acropolis_east_elevator_hall
extern GpRoomObjRec D_acropolis_east_elevator_hall_80186320[];

extern u8* D_acropolis_east_elevator_hall_80186330[];

extern GpViewCountRec D_acropolis_east_elevator_hall_80186334[];

extern GpRoomCoordRec D_acropolis_east_elevator_hall_80186338[];

extern GpWarpRec D_acropolis_east_elevator_hall_80186340[];

extern GpSprtRec D_acropolis_east_elevator_hall_80187870[];

extern GpViewRec D_acropolis_east_elevator_hall_80187A5C[];

extern GpRoomParamRec* D_acropolis_east_elevator_hall_80187B74[];

void func_acropolis_east_elevator_hall_8017F5B4(Task* task);

void func_acropolis_east_elevator_hall_8017F77C(Task* arg0);

void func_acropolis_east_elevator_hall_8017F2F8(Task* task);

void func_acropolis_east_elevator_hall_8017F55C(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_EAST_ELEVATOR_HALL_H
