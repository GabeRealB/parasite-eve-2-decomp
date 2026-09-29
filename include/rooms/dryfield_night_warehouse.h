#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_WAREHOUSE_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_WAREHOUSE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_night_warehouse_8017FB98[12];

// dryfield_night_warehouse
extern GpRoomCoordRec D_dryfield_night_warehouse_8017E8E8[];

extern GpRoomObjRec D_dryfield_night_warehouse_8017E900[];

extern u8* D_dryfield_night_warehouse_8017E930[];

extern GpViewCountRec D_dryfield_night_warehouse_8017E93C[];

extern GpWarpRec D_dryfield_night_warehouse_8017E944[];

extern GpViewRec D_dryfield_night_warehouse_8017EF2C[];

extern GpSprtRec D_dryfield_night_warehouse_8017F46C[];

extern GpRoomParamRec* D_dryfield_night_warehouse_8017FC24[];

void func_dryfield_night_warehouse_8017E778(Task* arg0);

void func_dryfield_night_warehouse_8017D65C(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_WAREHOUSE_H
