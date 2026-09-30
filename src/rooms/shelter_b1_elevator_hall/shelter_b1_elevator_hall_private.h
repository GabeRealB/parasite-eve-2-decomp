#ifndef SRC_ROOMS_SHELTER_B1_ELEVATOR_HALL_SHELTER_B1_ELEVATOR_HALL_PRIVATE_H
#define SRC_ROOMS_SHELTER_B1_ELEVATOR_HALL_SHELTER_B1_ELEVATOR_HALL_PRIVATE_H

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

#include "rooms/room_common.h"

extern TaskDesc D_shelter_b1_elevator_hall_80182CAC;

extern GpMsgEntry D_shelter_b1_elevator_hall_80182CB8[6];

extern TaskDesc D_shelter_b1_elevator_hall_80182CE8;

extern RoomFadeStorage D_shelter_b1_elevator_hall_801849F0;

// Callbacks referenced by the overlay's shared data tables.

s32 func_shelter_b1_elevator_hall_8017D810(Task*, s32, RoomEventMsg*, RoomEventMsg*);

void func_shelter_b1_elevator_hall_8017D99C(Task*);

s32 func_shelter_b1_elevator_hall_8017DB54(Task*, s32, TaskMessageArg, TaskMessageArg);

s32 func_shelter_b1_elevator_hall_8017DB5C(Task*, s32, TaskMessageArg, TaskMessageArg);

s32 func_shelter_b1_elevator_hall_8017DB64(Task*, s32, TaskMessageArg, TaskMessageArg);

s32 func_shelter_b1_elevator_hall_8017DB6C(Task*, s32, s32, TaskMessageArg);

#endif // SRC_ROOMS_SHELTER_B1_ELEVATOR_HALL_SHELTER_B1_ELEVATOR_HALL_PRIVATE_H
