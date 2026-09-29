#ifndef INCLUDE_ROOMS_DRYFIELD_GENERAL_STORE_H
#define INCLUDE_ROOMS_DRYFIELD_GENERAL_STORE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_general_store_80185654[13];

// dryfield_general_store
extern GpRoomObjRec D_dryfield_general_store_8017E670[];

extern u8* D_dryfield_general_store_8017E680[];

extern GpViewCountRec D_dryfield_general_store_8017E684[];

extern GpRoomCoordRec D_dryfield_general_store_8017E688[];

extern GpWarpRec D_dryfield_general_store_8017E690[];

extern GpViewRec D_dryfield_general_store_8017F25C[];

extern GpSprtRec D_dryfield_general_store_8018402C[];

extern GpRoomParamRec* D_dryfield_general_store_801856D8[];

void func_dryfield_general_store_8017E150(Task* unused);

void func_dryfield_general_store_8017DF5C(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_GENERAL_STORE_H
