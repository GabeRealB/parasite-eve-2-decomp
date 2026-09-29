#ifndef ACROPOLIS_HELICOPTER_LANDING_PAD_PRIVATE_H
#define ACROPOLIS_HELICOPTER_LANDING_PAD_PRIVATE_H

#include "main/task_types.h"

#include "gameplay/message.h"

#include "gameplay/direction.h"

#include "common.h"

// Callbacks referenced by the overlay's shared data tables.
void func_acropolis_helicopter_landing_pad_8017DA9C(Task *);
void func_acropolis_helicopter_landing_pad_8017DE78(Task *);
void func_acropolis_helicopter_landing_pad_8017DFCC(Task *);
void func_acropolis_helicopter_landing_pad_8017E0F8(Task *);
void func_acropolis_helicopter_landing_pad_8017E270(Task *);
s32 func_acropolis_helicopter_landing_pad_8017E3F0(Task *, s32, GpSaveLoc *, GpSaveLoc *);
s32 func_acropolis_helicopter_landing_pad_8017E49C(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_acropolis_helicopter_landing_pad_8017E4A4(Task *, s32, GpMsg13EF *, GpMessageArg);
s32 func_acropolis_helicopter_landing_pad_8017E570(Task *, s32, s32, GpMessageArg);
void func_acropolis_helicopter_landing_pad_8017E5B8(void);
void func_acropolis_helicopter_landing_pad_8017E5E8(void);
void func_acropolis_helicopter_landing_pad_8017E64C(void);
void func_acropolis_helicopter_landing_pad_8017E67C(void);
void func_acropolis_helicopter_landing_pad_8017E6C0(s32);
void func_acropolis_helicopter_landing_pad_8017E6F0(void);
void func_acropolis_helicopter_landing_pad_8017E724(void);
void func_acropolis_helicopter_landing_pad_8017E75C(s32);
void func_acropolis_helicopter_landing_pad_8017E76C(Task *);
void func_acropolis_helicopter_landing_pad_8017E81C(Task *);
void func_acropolis_helicopter_landing_pad_8017E974(Task *);

#endif // ACROPOLIS_HELICOPTER_LANDING_PAD_PRIVATE_H
