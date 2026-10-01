#ifndef INCLUDE_ROOMS_SHELTER_B2_POD_BOTTOM_H
#define INCLUDE_ROOMS_SHELTER_B2_POD_BOTTOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TmdSource D_shelter_b2_pod_bottom_80187B10;

extern TmdSource D_shelter_b2_pod_bottom_80187E14;

extern TmdSource D_shelter_b2_pod_bottom_80188264;

extern TmdSource D_shelter_b2_pod_bottom_8018864C;

extern GpAreaVariant D_shelter_b2_pod_bottom_80187678[11];

// shelter_b2_pod_bottom
extern GpRoomObjRec D_shelter_b2_pod_bottom_80181D14[];

extern WorldCoordRoomLighting D_shelter_b2_pod_bottom_80181D24[];

extern u8* D_shelter_b2_pod_bottom_80181D2C[];

extern GpViewCountRec D_shelter_b2_pod_bottom_80181D30[];

extern GpWarpRec D_shelter_b2_pod_bottom_80181D34[];

extern GpViewRec D_shelter_b2_pod_bottom_80182B80[];

extern GpSprtRec D_shelter_b2_pod_bottom_80185904[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_pod_bottom_80188770[];

void func_shelter_b2_pod_bottom_8017D708(Task* task);

void func_shelter_b2_pod_bottom_80181A48(Task* task);

void func_shelter_b2_pod_bottom_80181940(Task* arg0);

void func_shelter_b2_pod_bottom_8017D850(Task* task);

void func_shelter_b2_pod_bottom_80181B48(Task* arg0);

void func_shelter_b2_pod_bottom_8018016C(Task* task);

void func_shelter_b2_pod_bottom_80180898(Task* task);

void func_shelter_b2_pod_bottom_80180F10(Task* arg0);

void func_shelter_b2_pod_bottom_8017D760(Task* task);

void func_shelter_b2_pod_bottom_8017EC78(Task* task);

void func_shelter_b2_pod_bottom_8017F448(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B2_POD_BOTTOM_H
