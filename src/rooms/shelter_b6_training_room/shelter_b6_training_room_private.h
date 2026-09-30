#ifndef SRC_ROOMS_SHELTER_B6_TRAINING_ROOM_SHELTER_B6_TRAINING_ROOM_PRIVATE_H
#define SRC_ROOMS_SHELTER_B6_TRAINING_ROOM_SHELTER_B6_TRAINING_ROOM_PRIVATE_H

#include "types.h"

#include "gameplay/evs.h"
#include "gameplay/message.h"

#include "main/coord.h"
#include "main/task_types.h"

extern u8 D_shelter_b6_training_room_80185C60[3][16];

extern GfxCoord* D_shelter_b6_training_room_80185C90;

extern GfxCoord* D_shelter_b6_training_room_80185C94;

extern u16 D_shelter_b6_training_room_80185C98;

extern GpMsgEntry D_shelter_b6_training_room_80182AF4[6];

extern s32 D_shelter_b6_training_room_80182B24;

extern TaskDesc D_shelter_b6_training_room_801839A8;

extern GpEvsCmd D_shelter_b6_training_room_80183BB4[58];

extern GpEvsCmd D_shelter_b6_training_room_80184124[14];

extern GpEvsCmd D_shelter_b6_training_room_80184274[7];

extern TaskDesc D_shelter_b6_training_room_8018431C[2];

// Callbacks referenced by the overlay's shared data tables.
s32 func_shelter_b6_training_room_8017D638(Task*, s32, TaskMessageArg, TaskMessageArg);

s32 func_shelter_b6_training_room_8017D640(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_shelter_b6_training_room_8017D684(Task*, s32, s32, TaskMessageArg);

s32 func_shelter_b6_training_room_8017D75C(Task*, s32, TaskMessageArg, TaskMessageArg);

s32 func_shelter_b6_training_room_8017D764(Task*, s32, TaskMessageArg, TaskMessageArg);

void func_shelter_b6_training_room_8017D940(void);

void func_shelter_b6_training_room_8017D974(s32);

void func_shelter_b6_training_room_8017D9C8(Task*);

void func_shelter_b6_training_room_8017DAC8(void);

void func_shelter_b6_training_room_8017DAF8(s32);

void func_shelter_b6_training_room_8017DB28(void);

void func_shelter_b6_training_room_8017DB70(void);

void func_shelter_b6_training_room_8017DBBC(Task*);

void func_shelter_b6_training_room_8017DD98(Task*);

#endif // SRC_ROOMS_SHELTER_B6_TRAINING_ROOM_SHELTER_B6_TRAINING_ROOM_PRIVATE_H
