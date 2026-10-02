#ifndef INCLUDE_ROOMS_ACROPOLIS_PATIO_H
#define INCLUDE_ROOMS_ACROPOLIS_PATIO_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_acropolis_patio_80184A90[12];

// acropolis_patio
extern GpRoomObjRec D_acropolis_patio_80182E68[];

extern u8* D_acropolis_patio_80182EC0[];

extern GpViewCountRec D_acropolis_patio_80182ECC[];

extern WorldCoordRoomLighting D_acropolis_patio_80182ED4[];

extern GpWarpRec D_acropolis_patio_80182EEC[];

extern SpriteView D_acropolis_patio_80186360[];

extern ViewCamera D_acropolis_patio_80186D5C[];

extern WorldCollisionSurfaceProperties* D_acropolis_patio_8018703C[];

void func_acropolis_patio_8017E100(Task* task);

void func_acropolis_patio_8017E324(Task* task);

void func_acropolis_patio_8017E730(Task* task);

void func_acropolis_patio_8017DF8C(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_PATIO_H
