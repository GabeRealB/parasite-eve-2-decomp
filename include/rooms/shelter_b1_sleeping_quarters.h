#ifndef INCLUDE_ROOMS_SHELTER_B1_SLEEPING_QUARTERS_H
#define INCLUDE_ROOMS_SHELTER_B1_SLEEPING_QUARTERS_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TmdSource D_shelter_b1_sleeping_quarters_801804F4;

extern GpAreaVariant D_shelter_b1_sleeping_quarters_80183FDC[22];

// shelter_b1_sleeping_quarters
extern u8* D_shelter_b1_sleeping_quarters_80180658[];

extern GpViewCountRec D_shelter_b1_sleeping_quarters_8018065C[];

extern GpWarpRec D_shelter_b1_sleeping_quarters_80180660[];

extern WorldCollisionGrid D_shelter_b1_sleeping_quarters_801810D4;

extern GpViewRec D_shelter_b1_sleeping_quarters_801810F8[];

extern GpSprtRec D_shelter_b1_sleeping_quarters_80182E70[];

extern WorldCoordRoomLights D_shelter_b1_sleeping_quarters_80183234;

extern GpObj4A D_shelter_b1_sleeping_quarters_8018324C[];

extern GpObj3A D_shelter_b1_sleeping_quarters_801837A4[];

extern GpObj4A D_shelter_b1_sleeping_quarters_801838D0[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_sleeping_quarters_801840B0[];

void func_shelter_b1_sleeping_quarters_8017D608(Task* task);

void func_shelter_b1_sleeping_quarters_8017D888(Task* task);

void func_shelter_b1_sleeping_quarters_8017D8E0(Task* arg0);

void func_shelter_b1_sleeping_quarters_8017E6DC(Task* arg0);

void func_shelter_b1_sleeping_quarters_8017F894(Task* arg0);

void func_shelter_b1_sleeping_quarters_8017EC34(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_SLEEPING_QUARTERS_H
