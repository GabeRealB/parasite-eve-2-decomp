#ifndef DRYFIELD_FACTORY_PRIVATE_H
#define DRYFIELD_FACTORY_PRIVATE_H

#include "main/task_types.h"

#include "gameplay/message.h"

#include "gameplay/direction.h"

#include "common.h"

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_factory_8017D85C(Task *);
s32 func_dryfield_factory_8017DB08(Task *, s32, RoomEventMsg *, RoomEventMsg *);
void func_dryfield_factory_8017DD00(Task *);
s32 func_dryfield_factory_8017DDA0(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_factory_8017DDA8(Task *, s32, s32, GpMessageArg);
s32 func_dryfield_factory_8017DEA8(Task *, s32, s32, s32);
s32 func_dryfield_factory_8017DF14(Task *, s32, GpMsg13EF *, GpMessageArg);
void func_dryfield_factory_8017FC18(Task *);
void func_dryfield_factory_8017FDDC(Task *);
void func_dryfield_factory_8018001C(Task *);
void func_dryfield_factory_8018072C(Task *);
void func_dryfield_factory_80180784(Task *);
void func_dryfield_factory_801807DC(Task *);
void func_dryfield_factory_80180920(Task *);
void func_dryfield_factory_80180964(Task *);

#endif // DRYFIELD_FACTORY_PRIVATE_H
