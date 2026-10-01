#ifndef INCLUDE_ROOMS_SHELTER_R48_H
#define INCLUDE_ROOMS_SHELTER_R48_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpGridParams D_shelter_r48_80183EEC[1];

extern TaskDesc D_shelter_r48_80182FAC;

extern GpAreaVariant D_shelter_r48_8018BC10[13];

// shelter_r48
extern GpRoomCoordRec D_shelter_r48_80183014[];

extern GpRoomObjRec D_shelter_r48_8018301C[];

extern u8* D_shelter_r48_8018302C[];

extern GpViewCountRec D_shelter_r48_80183030[];

extern GpWarpRec D_shelter_r48_80183034[];

extern GpViewRec D_shelter_r48_80183F10[];

extern GpSprtRec D_shelter_r48_80189FB4[];

extern WorldCollisionSurfaceProperties* D_shelter_r48_8018BE10[];

void func_shelter_r48_8017E224(Task* task);

void func_shelter_r48_8017E27C(u8 arg0);

void func_shelter_r48_8017E3B8(Task* task);

void func_shelter_r48_8017E4C4(Task* arg0);

void func_shelter_r48_8017EC18(Task* task);

void func_shelter_r48_8017F6C0(Task* task);

void func_shelter_r48_80180210(Task* task);

void func_shelter_r48_8017E704(Task* arg0);

void func_shelter_r48_8017E9B8(Task* arg0);

void func_shelter_r48_8017EFD8(Task* task);

void func_shelter_r48_801810B0(Task* task);

void func_shelter_r48_8018147C(Task* task);

void func_shelter_r48_80181704(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_R48_H
