#ifndef SHELTER_B6_TRAINING_ROOM_PRIVATE_H
#define SHELTER_B6_TRAINING_ROOM_PRIVATE_H

#include "main/task_types.h"

#include "gameplay/message.h"

#include "gameplay/direction.h"

#include "common.h"

// Callbacks referenced by the overlay's shared data tables.
s32 func_shelter_b6_training_room_8017D638(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_shelter_b6_training_room_8017D640(Task *, s32, GpSaveLoc *, GpSaveLoc *);
s32 func_shelter_b6_training_room_8017D684(Task *, s32, s32, GpMessageArg);
s32 func_shelter_b6_training_room_8017D75C(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_shelter_b6_training_room_8017D764(Task *, s32, GpMessageArg, GpMessageArg);
void func_shelter_b6_training_room_8017D940(void);
void func_shelter_b6_training_room_8017D974(s32);
void func_shelter_b6_training_room_8017D9C8(Task *);
void func_shelter_b6_training_room_8017DAC8(void);
void func_shelter_b6_training_room_8017DAF8(s32);
void func_shelter_b6_training_room_8017DB28(void);
void func_shelter_b6_training_room_8017DB70(void);
void func_shelter_b6_training_room_8017DBBC(Task *);
void func_shelter_b6_training_room_8017DD98(Task *);

#endif // SHELTER_B6_TRAINING_ROOM_PRIVATE_H
