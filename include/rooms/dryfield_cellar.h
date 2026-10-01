#ifndef INCLUDE_ROOMS_DRYFIELD_CELLAR_H
#define INCLUDE_ROOMS_DRYFIELD_CELLAR_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_cellar_80180ACC[12];

// dryfield_cellar
extern GpRoomObjRec D_dryfield_cellar_8017DBDC[];

extern u8* D_dryfield_cellar_8017DC04[];

extern GpViewCountRec D_dryfield_cellar_8017DC0C[];

extern GpRoomCoordRec D_dryfield_cellar_8017DC10[];

extern GpWarpRec D_dryfield_cellar_8017DC20[];

extern GpViewRec D_dryfield_cellar_8017DF78[];

extern GpSprtRec D_dryfield_cellar_8017FE40[];

extern WorldCollisionSurfaceProperties* D_dryfield_cellar_80180B40[];

void func_dryfield_cellar_8017DAEC(Task* arg0);

void func_dryfield_cellar_8017D784(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_CELLAR_H
