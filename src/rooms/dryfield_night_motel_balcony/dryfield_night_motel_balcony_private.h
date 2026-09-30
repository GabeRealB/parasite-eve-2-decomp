#ifndef SRC_ROOMS_DRYFIELD_NIGHT_MOTEL_BALCONY_DRYFIELD_NIGHT_MOTEL_BALCONY_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_MOTEL_BALCONY_DRYFIELD_NIGHT_MOTEL_BALCONY_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

extern TaskDesc gRoomEventTaskDesc;

extern GpMsgEntry D_dryfield_night_motel_balcony_80182804[6];

extern SVECTOR D_dryfield_night_motel_balcony_80182C60[2];

extern SVECTOR D_dryfield_night_motel_balcony_80182C70;

extern SVECTOR D_dryfield_night_motel_balcony_80182C80;

extern SVECTOR D_dryfield_night_motel_balcony_80182C90;

// Callbacks referenced by the overlay's shared data tables.

s32 func_dryfield_night_motel_balcony_8017DC18(Task*, s32, TaskMessageArg, TaskMessageArg);

s32 func_dryfield_night_motel_balcony_8017DC20(Task*, s32, TaskMessageArg, TaskMessageArg);

s32 func_dryfield_night_motel_balcony_8017DC28(Task*, s32, TaskMessageArg, TaskMessageArg);

void func_dryfield_night_motel_balcony_8017E0C8(Task*);

#endif // SRC_ROOMS_DRYFIELD_NIGHT_MOTEL_BALCONY_DRYFIELD_NIGHT_MOTEL_BALCONY_PRIVATE_H
