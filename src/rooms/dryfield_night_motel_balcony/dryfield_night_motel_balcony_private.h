#ifndef DRYFIELD_NIGHT_MOTEL_BALCONY_PRIVATE_H
#define DRYFIELD_NIGHT_MOTEL_BALCONY_PRIVATE_H

#include "main/task_types.h"

#include "gameplay/message.h"

#include "common.h"

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_night_motel_balcony_8017D7F8(Task *);
s32 func_dryfield_night_motel_balcony_8017D968(Task *, s32, RoomEventMsg *, RoomEventMsg *);
s32 func_dryfield_night_motel_balcony_8017DBC8(Task *, s32, s32, s32);
s32 func_dryfield_night_motel_balcony_8017DC18(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_night_motel_balcony_8017DC20(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_night_motel_balcony_8017DC28(Task *, s32, GpMessageArg, GpMessageArg);
void func_dryfield_night_motel_balcony_8017E0C8(Task *);

#endif // DRYFIELD_NIGHT_MOTEL_BALCONY_PRIVATE_H
