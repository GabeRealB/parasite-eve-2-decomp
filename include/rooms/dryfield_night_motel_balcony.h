#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_BALCONY_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_BALCONY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_dryfield_night_motel_balcony_80182834[2];

extern GpAreaApplyRec D_dryfield_night_motel_balcony_8018F2CC[2];

extern GpAreaVariant D_dryfield_night_motel_balcony_8018EA94[13];

// dryfield_night_motel_balcony
extern WorldCoordRoomLighting D_dryfield_night_motel_balcony_80182E00[];

extern GpRoomObjRec D_dryfield_night_motel_balcony_80182E18[];

extern u8* D_dryfield_night_motel_balcony_80182E98[];

extern GpViewCountRec D_dryfield_night_motel_balcony_80182EA4[];

extern GpWarpRec D_dryfield_night_motel_balcony_80182EAC[];

extern ViewCamera D_dryfield_night_motel_balcony_80184004[];

extern SpriteView D_dryfield_night_motel_balcony_8018D078[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_motel_balcony_8018F2AC[];

void func_dryfield_night_motel_balcony_80182730(void);

void func_dryfield_night_motel_balcony_8018257C(void);

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

void func_dryfield_night_motel_balcony_8017DD78(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_BALCONY_H
