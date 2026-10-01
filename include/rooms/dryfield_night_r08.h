#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_R08_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_R08_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_night_r08_801818D8[11];

// dryfield_night_r08
extern GpRoomCoordRec D_dryfield_night_r08_8018067C[];

extern GpRoomObjRec D_dryfield_night_r08_80180684[];

extern u8* D_dryfield_night_r08_80180694[];

extern GpViewCountRec D_dryfield_night_r08_80180698[];

extern GpWarpRec D_dryfield_night_r08_8018069C[];

extern GpViewRec D_dryfield_night_r08_80181498[];

extern GpSprtRec D_dryfield_night_r08_80181728[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_r08_8018195C[];

void func_dryfield_night_r08_8017D718(Task* arg0);

void func_dryfield_night_r08_8017E5B0(Task* task);

void func_dryfield_night_r08_8017F014(Task* task);

void func_dryfield_night_r08_8017F8FC(Task* task);

void func_dryfield_night_r08_8017D6C0(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_R08_H
