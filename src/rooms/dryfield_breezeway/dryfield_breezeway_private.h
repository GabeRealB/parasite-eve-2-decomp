#ifndef DRYFIELD_BREEZEWAY_PRIVATE_H
#define DRYFIELD_BREEZEWAY_PRIVATE_H

#include "main/task_types.h"

#include "gameplay/message.h"

#include "common.h"

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_breezeway_8017D79C(Task *);
s32 func_dryfield_breezeway_8017D90C(Task *, s32, s32, s32);
s32 func_dryfield_breezeway_8017D940(Task *, s32, RoomEventMsg *, RoomEventMsg *);
s32 func_dryfield_breezeway_8017DA48(Task *, s32, s32, s32);
s32 func_dryfield_breezeway_8017DBA4(Task *, s32, s32, s32);
s32 func_dryfield_breezeway_8017DBD8(Task *, s32, RoomEventMsg *, RoomEventMsg *);
void func_dryfield_breezeway_8017DCE4(Task *);

#endif // DRYFIELD_BREEZEWAY_PRIVATE_H
