#ifndef SHELTER_B1_ELEVATOR_HALL_PRIVATE_H
#define SHELTER_B1_ELEVATOR_HALL_PRIVATE_H

#include "main/task_types.h"

#include "gameplay/message.h"

#include "gameplay/direction.h"

#include "common.h"

// Callbacks referenced by the overlay's shared data tables.
void func_shelter_b1_elevator_hall_8017D620(Task *);
s32 func_shelter_b1_elevator_hall_8017D810(Task *, s32, GpSaveLoc *, GpSaveLoc *);
void func_shelter_b1_elevator_hall_8017D99C(Task *);
s32 func_shelter_b1_elevator_hall_8017DB54(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_shelter_b1_elevator_hall_8017DB5C(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_shelter_b1_elevator_hall_8017DB64(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_shelter_b1_elevator_hall_8017DB6C(Task *, s32, s32, GpMessageArg);

#endif // SHELTER_B1_ELEVATOR_HALL_PRIVATE_H
