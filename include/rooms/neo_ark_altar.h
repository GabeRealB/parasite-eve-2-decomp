#ifndef INCLUDE_ROOMS_NEO_ARK_ALTAR_H
#define INCLUDE_ROOMS_NEO_ARK_ALTAR_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// neo_ark_altar
extern GpRoomObjRec D_neo_ark_altar_8017F094[];

extern WorldCoordRoomLighting D_neo_ark_altar_8017F0C4[];

extern u8* D_neo_ark_altar_8017F0EC[];

extern GpViewCountRec D_neo_ark_altar_8017F0F8[];

extern GpWarpRec D_neo_ark_altar_8017F0FC[];

extern GpViewRec D_neo_ark_altar_8017F5A0[];

extern GpSprtRec D_neo_ark_altar_8017FE38[];

extern WorldCollisionSurfaceProperties* D_neo_ark_altar_8018005C[];

void func_neo_ark_altar_8017EF84(Task* unused);

void func_neo_ark_altar_8017D9E8(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_ALTAR_H
