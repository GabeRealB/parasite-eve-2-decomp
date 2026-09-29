#ifndef INCLUDE_ROOMS_NEO_ARK_PYRAMID_H
#define INCLUDE_ROOMS_NEO_ARK_PYRAMID_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_neo_ark_pyramid_80181728[13];

// neo_ark_pyramid
extern GpRoomObjRec D_neo_ark_pyramid_8017FC28[];

extern GpRoomCoordRec D_neo_ark_pyramid_8017FC48[];

extern u8* D_neo_ark_pyramid_8017FC60[];

extern GpViewCountRec D_neo_ark_pyramid_8017FC68[];

extern GpWarpRec D_neo_ark_pyramid_8017FC6C[];

extern GpViewRec D_neo_ark_pyramid_801802E8[];

extern GpSprtRec D_neo_ark_pyramid_80180E18[];

extern GpRoomParamRec* D_neo_ark_pyramid_80181884[];

void func_neo_ark_pyramid_8017DC50(Task* task);

void func_neo_ark_pyramid_8017E6B4(Task* task);

void func_neo_ark_pyramid_8017EF9C(Task* task);

void func_neo_ark_pyramid_8017DBF0(Task* arg0);

void func_neo_ark_pyramid_8017DB98(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_PYRAMID_H
