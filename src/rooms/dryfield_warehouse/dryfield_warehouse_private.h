#ifndef DRYFIELD_WAREHOUSE_PRIVATE_H
#define DRYFIELD_WAREHOUSE_PRIVATE_H

#include "main/task_types.h"

#include "gameplay/message.h"

#include "common.h"

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_warehouse_8017D5E8(Task *);
s32 func_dryfield_warehouse_8017D764(Task *, s32, s32, GpMessageArg);
s32 func_dryfield_warehouse_8017D824(Task *, s32, RoomEventMsg *, RoomEventMsg *);
void func_dryfield_warehouse_8017D8D4(Task *);

#endif // DRYFIELD_WAREHOUSE_PRIVATE_H
