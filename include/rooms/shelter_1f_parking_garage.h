#ifndef INCLUDE_ROOMS_SHELTER_1F_PARKING_GARAGE_H
#define INCLUDE_ROOMS_SHELTER_1F_PARKING_GARAGE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_shelter_1f_parking_garage_801818D8[12];

// shelter_1f_parking_garage
extern GpRoomObjRec D_shelter_1f_parking_garage_80180C64[];

extern GpRoomCoordRec D_shelter_1f_parking_garage_80180C74[];

extern u8* D_shelter_1f_parking_garage_80180C7C[];

extern GpViewCountRec D_shelter_1f_parking_garage_80180C80[];

extern GpWarpRec D_shelter_1f_parking_garage_80180C84[];

extern GpViewRec D_shelter_1f_parking_garage_8018100C[];

extern GpSprtRec D_shelter_1f_parking_garage_80181430[];

extern WorldCollisionSurfaceProperties* D_shelter_1f_parking_garage_80181954[];

/// Task entries the Neo Ark map UI overlay's stage tables name: each room's
/// entry task, started for its location, and the enemy descriptors' tasks.
void func_shelter_1f_parking_garage_8017DF14(Task* task);

void func_shelter_1f_parking_garage_8017DF6C(Task* arg0);

void func_shelter_1f_parking_garage_8017EC0C(Task* task);

void func_shelter_1f_parking_garage_8017F670(Task* task);

void func_shelter_1f_parking_garage_8017FF58(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_1F_PARKING_GARAGE_H
