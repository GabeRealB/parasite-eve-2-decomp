#ifndef INCLUDE_ROOMS_DRYFIELD_MOTEL_BALCONY_H
#define INCLUDE_ROOMS_DRYFIELD_MOTEL_BALCONY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_motel_balcony_80186220[13];

// dryfield_motel_balcony
extern GpRoomObjRec D_dryfield_motel_balcony_801822D0[];

extern u8* D_dryfield_motel_balcony_801822E0[];

extern GpViewCountRec D_dryfield_motel_balcony_801822E4[];

extern GpRoomCoordRec D_dryfield_motel_balcony_801822E8[];

extern GpWarpRec D_dryfield_motel_balcony_801822F0[];

extern GpViewRec D_dryfield_motel_balcony_80182B80[];

extern GpSprtRec D_dryfield_motel_balcony_80185C98[];

extern GpRoomParamRec* D_dryfield_motel_balcony_80186704[];

void func_dryfield_motel_balcony_801802DC(Task* arg0);

void func_dryfield_motel_balcony_80180D40(Task* task);

void func_dryfield_motel_balcony_80181628(Task* task);

void func_dryfield_motel_balcony_8017DCB8(Task* task);

void func_dryfield_motel_balcony_8017EA00(Task* arg0);

void func_dryfield_motel_balcony_8017ED98(Task* arg0);

void func_dryfield_motel_balcony_801801A8(Task* arg0);

void func_dryfield_motel_balcony_8017DC28(Task* arg0);

void func_dryfield_motel_balcony_8017DBD0(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MOTEL_BALCONY_H
