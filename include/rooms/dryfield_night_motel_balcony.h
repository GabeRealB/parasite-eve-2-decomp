#ifndef ROOMS_DRYFIELD_NIGHT_MOTEL_BALCONY_H
#define ROOMS_DRYFIELD_NIGHT_MOTEL_BALCONY_H

#include "gameplay/area.h"

#include "gameplay/area_flags.h"

#include "main/task_types.h"

#include "types.h"

#include "common.h"

/// Reapplies the room's nine switchable sprite-command sets from their saved
/// flag nibbles 0x85-0x8D.

void func_dryfield_night_motel_balcony_80182730(void);

void func_dryfield_night_motel_balcony_8018257C(void);

extern TaskDesc D_dryfield_night_motel_balcony_80182834[2];

extern GpAreaApplyRec D_dryfield_night_motel_balcony_8018F2CC[2];

void func_dryfield_night_motel_balcony_8017E128(u8 arg0);

void func_dryfield_night_motel_balcony_8017E250(s16 arg0, s16 arg1);

void func_dryfield_night_motel_balcony_8017E3C8(void);

void func_dryfield_night_motel_balcony_8017E4B8(void);

void func_dryfield_night_motel_balcony_8017F6C8(s32 arg0, s16 arg1, s16 arg2, s16 arg3);

void func_dryfield_night_motel_balcony_8017F84C(Task* task);
void func_dryfield_night_motel_balcony_80180580(Task* task);
void func_dryfield_night_motel_balcony_80181E7C(Task* task);
void func_dryfield_night_motel_balcony_801809CC(Task* task);
void func_dryfield_night_motel_balcony_80181024(Task* task);
void func_dryfield_night_motel_balcony_8018158C(Task* task);
void func_dryfield_night_motel_balcony_8017E554(Task* task);
extern GpAreaVariant D_dryfield_night_motel_balcony_8018EA94[13];

#endif
