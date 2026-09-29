#ifndef INCLUDE_ROOMS_NEO_ARK_R31_H
#define INCLUDE_ROOMS_NEO_ARK_R31_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern s32 D_neo_ark_r31_8017DC54;

extern TaskDesc D_neo_ark_r31_8017D9E8;

extern GpAreaVariant D_neo_ark_r31_8017DBB8[13];

// neo_ark_r31
extern u8* D_neo_ark_r31_8017DA1C[];

extern GpViewCountRec D_neo_ark_r31_8017DA20[];

extern GpWarpRec D_neo_ark_r31_8017DA24[];

extern GpViewRec D_neo_ark_r31_8017DA5C[];

extern GpSprtRec D_neo_ark_r31_8017DAF8[];

extern GpRoomCoordSet D_neo_ark_r31_8017DB7C;

extern GpRoomParamRec* D_neo_ark_r31_8017DC34[];

void func_neo_ark_r31_8017D990(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_R31_H
