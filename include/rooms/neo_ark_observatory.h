#ifndef INCLUDE_ROOMS_NEO_ARK_OBSERVATORY_H
#define INCLUDE_ROOMS_NEO_ARK_OBSERVATORY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_neo_ark_observatory_80180DBC[2];

extern GpAreaVariant D_neo_ark_observatory_8018786C[13];

// neo_ark_observatory
extern GpRoomObjRec D_neo_ark_observatory_80181594[];

extern GpRoomCoordRec D_neo_ark_observatory_801815B4[];

extern u8* D_neo_ark_observatory_801815DC[];

extern GpViewCountRec D_neo_ark_observatory_801815E4[];

extern GpWarpRec D_neo_ark_observatory_801815E8[];

extern GpViewRec D_neo_ark_observatory_80181FC8[];

extern GpSprtRec D_neo_ark_observatory_801860E8[];

extern GpRoomParamRec* D_neo_ark_observatory_80187A08[];

void func_neo_ark_observatory_80180DAC(s32 arg0);

void func_neo_ark_observatory_8017FA98(s32 arg0);

void func_neo_ark_observatory_80180124(Task* task);

void func_neo_ark_observatory_8017FDDC(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_OBSERVATORY_H
