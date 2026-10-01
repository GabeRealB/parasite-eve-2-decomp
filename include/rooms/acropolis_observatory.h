#ifndef INCLUDE_ROOMS_ACROPOLIS_OBSERVATORY_H
#define INCLUDE_ROOMS_ACROPOLIS_OBSERVATORY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_acropolis_observatory_80181264[19];

// acropolis_observatory
extern GpRoomObjRec D_acropolis_observatory_8017FEC8[];

extern u8* D_acropolis_observatory_8017FEF0[];

extern GpViewCountRec D_acropolis_observatory_8017FEF8[];

extern WorldCoordRoomLighting D_acropolis_observatory_8017FEFC[];

extern GpWarpRec D_acropolis_observatory_8017FF0C[];

extern GpSprtRec D_acropolis_observatory_80183300[];

extern GpViewRec D_acropolis_observatory_80183360[];

extern WorldCollisionSurfaceProperties* D_acropolis_observatory_801834DC[];

void func_acropolis_observatory_8017E6F8(Task* task);

void func_acropolis_observatory_8017E424(Task* arg0);

void func_acropolis_observatory_8017D950(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_OBSERVATORY_H
