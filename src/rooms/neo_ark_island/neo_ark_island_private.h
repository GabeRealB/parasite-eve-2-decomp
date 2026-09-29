#ifndef NEO_ARK_ISLAND_PRIVATE_H
#define NEO_ARK_ISLAND_PRIVATE_H

#include "main/task_types.h"

#include "gameplay/message.h"

#include "gameplay/direction.h"

#include "common.h"

// Callbacks referenced by the overlay's shared data tables.
void func_neo_ark_island_8017D650(Task *);
void func_neo_ark_island_8017E2A4(Task *);
void func_neo_ark_island_8017E844(Task *);
s32 func_neo_ark_island_8017E960(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_neo_ark_island_8017E968(Task *, s32, GpSaveLoc *, GpSaveLoc *);
s32 func_neo_ark_island_8017EA24(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_neo_ark_island_8017EA2C(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_neo_ark_island_8017EA34(Task *, s32, s32, GpMessageArg);

#endif // NEO_ARK_ISLAND_PRIVATE_H
