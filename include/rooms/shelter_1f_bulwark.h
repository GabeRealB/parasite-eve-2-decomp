#ifndef INCLUDE_ROOMS_SHELTER_1F_BULWARK_H
#define INCLUDE_ROOMS_SHELTER_1F_BULWARK_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_shelter_1f_bulwark_80180DA8[12];

// shelter_1f_bulwark
extern GpRoomObjRec D_shelter_1f_bulwark_801803B0[];

extern GpRoomCoordRec D_shelter_1f_bulwark_801803C0[];

extern u8* D_shelter_1f_bulwark_801803C8[];

extern GpViewCountRec D_shelter_1f_bulwark_801803CC[];

extern GpWarpRec D_shelter_1f_bulwark_801803D0[];

extern GpViewRec D_shelter_1f_bulwark_8018066C[];

extern GpSprtRec D_shelter_1f_bulwark_801807B0[];

extern WorldCollisionSurfaceProperties* D_shelter_1f_bulwark_80180E9C[];

void func_shelter_1f_bulwark_8017DC20(Task* task);

void func_shelter_1f_bulwark_8017E38C(Task* task);

void func_shelter_1f_bulwark_8017EDF0(Task* task);

void func_shelter_1f_bulwark_8017F6D8(Task* task);

void func_shelter_1f_bulwark_8017E2A4(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_1F_BULWARK_H
