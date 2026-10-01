#ifndef INCLUDE_ROOMS_MINE_GORGE_H
#define INCLUDE_ROOMS_MINE_GORGE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_mine_gorge_80183544[12];

// mine_gorge
extern WorldCoordRoomLighting D_mine_gorge_8017E7A8[];

extern GpRoomObjRec D_mine_gorge_8017E7B8[];

extern u8* D_mine_gorge_8017E7E4[];

extern GpViewCountRec D_mine_gorge_8017E7EC[];

extern GpWarpRec D_mine_gorge_8017E7F0[];

extern GpViewRec D_mine_gorge_8017FA14[];

extern GpSprtRec D_mine_gorge_801827F8[];

extern WorldCollisionSurfaceProperties* D_mine_gorge_80183644[];

void func_mine_gorge_8017D9F8(Task* unused);

void func_mine_gorge_8017D9A0(Task* task);

#endif // INCLUDE_ROOMS_MINE_GORGE_H
