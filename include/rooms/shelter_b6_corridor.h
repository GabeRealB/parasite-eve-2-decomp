#ifndef INCLUDE_ROOMS_SHELTER_B6_CORRIDOR_H
#define INCLUDE_ROOMS_SHELTER_B6_CORRIDOR_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_shelter_b6_corridor_80180304[13];

// shelter_b6_corridor
extern u8* D_shelter_b6_corridor_8017F8B4[];

extern GpViewCountRec D_shelter_b6_corridor_8017F8B8[];

extern GpWarpRec D_shelter_b6_corridor_8017F8BC[];

extern GpGridParams D_shelter_b6_corridor_8017FA90;

extern GpViewRec D_shelter_b6_corridor_8017FAB4[];

extern GpSprtRec D_shelter_b6_corridor_8018004C[];

extern GpRoomCoordSet D_shelter_b6_corridor_801800E8;

extern GpObj4A D_shelter_b6_corridor_80180100[];

extern GpObj4A D_shelter_b6_corridor_8018036C[];

extern GpRoomBoundVec D_shelter_b6_corridor_801804E8[];

extern GpRoomParamRec* D_shelter_b6_corridor_80180548[];

void func_shelter_b6_corridor_8017E144(Task* task);

void func_shelter_b6_corridor_8017EBA4(Task* task);

void func_shelter_b6_corridor_8017EE08(s32 arg0, s32 arg1);

void func_shelter_b6_corridor_8017E238(Task* task);

void func_shelter_b6_corridor_8017ECA8(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B6_CORRIDOR_H
