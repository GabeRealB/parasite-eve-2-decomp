#ifndef INCLUDE_ROOMS_DRYFIELD_R08_H
#define INCLUDE_ROOMS_DRYFIELD_R08_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_r08_80180B88[13];

// dryfield_r08
extern GpRoomObjRec D_dryfield_r08_8017F6DC[];

extern u8* D_dryfield_r08_8017F6FC[];

extern GpViewCountRec D_dryfield_r08_8017F704[];

extern GpRoomCoordRec D_dryfield_r08_8017F708[];

extern GpWarpRec D_dryfield_r08_8017F718[];

extern GpViewRec D_dryfield_r08_8017FBBC[];

extern GpSprtRec D_dryfield_r08_80180918[];

extern GpRoomParamRec* D_dryfield_r08_80180C04[];

void func_dryfield_r08_8017F334(s32 arg0);

void func_dryfield_r08_8017F340(u8 arg0, u8 arg1);

void func_dryfield_r08_8017F438(s16 arg0);

void func_dryfield_r08_8017D5F8(Task* task);

void func_dryfield_r08_8017D8B4(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_R08_H
