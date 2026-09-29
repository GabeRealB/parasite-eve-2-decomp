#ifndef INCLUDE_ROOMS_SHELTER_B1_POD_SERVICE_GANTRY_H
#define INCLUDE_ROOMS_SHELTER_B1_POD_SERVICE_GANTRY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/task_types.h"

extern GpAreaVariant D_shelter_b1_pod_service_gantry_8017FBF8[11];

// shelter_b1_pod_service_gantry
extern u8* D_shelter_b1_pod_service_gantry_8017FB1C[];

extern GpViewCountRec D_shelter_b1_pod_service_gantry_8017FB20[];

extern GpWarpRec D_shelter_b1_pod_service_gantry_8017FB24[];

extern GpGridParams D_shelter_b1_pod_service_gantry_801801C4;

extern GpViewRec D_shelter_b1_pod_service_gantry_801801E8[];

extern GpSprtRec D_shelter_b1_pod_service_gantry_80181BA0[];

extern GpRoomCoordSet D_shelter_b1_pod_service_gantry_801824F4;

extern GpRoomParamRec* D_shelter_b1_pod_service_gantry_80182520[];

void func_shelter_b1_pod_service_gantry_8017D89C(Task* task);

void func_shelter_b1_pod_service_gantry_8017F450(GpCoord* arg0, s32 arg1, s32 arg2, s16 arg3);

void func_shelter_b1_pod_service_gantry_8017FA7C(Task* arg0);

void func_shelter_b1_pod_service_gantry_8017E880(Task* task);

void func_shelter_b1_pod_service_gantry_8017F8C8(Task* task);

void func_shelter_b1_pod_service_gantry_8017D8F4(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_POD_SERVICE_GANTRY_H
