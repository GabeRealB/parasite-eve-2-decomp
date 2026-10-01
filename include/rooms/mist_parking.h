#ifndef INCLUDE_ROOMS_MIST_PARKING_H
#define INCLUDE_ROOMS_MIST_PARKING_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_mist_parking_8018668C;

extern GpAreaVariant D_mist_parking_801951B4[13];

// mist_parking
extern GpRoomObjRec D_mist_parking_8019155C[];

extern u8* D_mist_parking_801915B0[];

extern GpViewCountRec D_mist_parking_801915C0[];

extern GpRoomCoordRec D_mist_parking_801915C8[];

extern GpWarpRec D_mist_parking_801915E8[];

extern GpViewRec D_mist_parking_80192228[];

extern GpSprtRec D_mist_parking_8019399C[];

extern WorldCollisionSurfaceProperties* D_mist_parking_801952F0[];

void func_mist_parking_80183BAC(s32 arg0);

void func_mist_parking_80181468(Task* task);

void func_mist_parking_80184728(Task* unused);

void func_mist_parking_80182898(Task* task);

#endif // INCLUDE_ROOMS_MIST_PARKING_H
