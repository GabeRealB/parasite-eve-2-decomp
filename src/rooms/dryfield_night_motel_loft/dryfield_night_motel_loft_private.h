#ifndef DRYFIELD_NIGHT_MOTEL_LOFT_PRIVATE_H
#define DRYFIELD_NIGHT_MOTEL_LOFT_PRIVATE_H

#include "main/task_types.h"

#include "gameplay/message.h"

#include "common.h"

// Callbacks referenced by the overlay's shared data tables.
s32 func_dryfield_night_motel_loft_8017D5F8(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_night_motel_loft_8017D600(Task *, s32, RoomEventMsg *, RoomEventMsg *);
s32 func_dryfield_night_motel_loft_8017D67C(Task *, s32, s32, s32);
s32 func_dryfield_night_motel_loft_8017D6BC(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_night_motel_loft_8017D6C4(Task *, s32, s32, s32);
void func_dryfield_night_motel_loft_8017D6F8(Task *);
void func_dryfield_night_motel_loft_8017D7EC(u8);

#endif // DRYFIELD_NIGHT_MOTEL_LOFT_PRIVATE_H
