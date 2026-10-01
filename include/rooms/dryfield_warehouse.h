#ifndef INCLUDE_ROOMS_DRYFIELD_WAREHOUSE_H
#define INCLUDE_ROOMS_DRYFIELD_WAREHOUSE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_warehouse_80182100[13];

// dryfield_warehouse
extern GpRoomObjRec D_dryfield_warehouse_8017FBBC[];

extern u8* D_dryfield_warehouse_8017FC04[];

extern GpViewCountRec D_dryfield_warehouse_8017FC10[];

extern WorldCoordRoomLighting D_dryfield_warehouse_8017FC18[];

extern GpWarpRec D_dryfield_warehouse_8017FC30[];

extern GpViewRec D_dryfield_warehouse_8018105C[];

extern GpSprtRec D_dryfield_warehouse_80181638[];

extern WorldCollisionSurfaceProperties* D_dryfield_warehouse_80182194[];

void func_dryfield_warehouse_8017F494(Task* arg0);

void func_dryfield_warehouse_8017DA00(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_WAREHOUSE_H
