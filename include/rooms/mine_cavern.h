#ifndef INCLUDE_ROOMS_MINE_CAVERN_H
#define INCLUDE_ROOMS_MINE_CAVERN_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_mine_cavern_8018E238[22];

// mine_cavern
extern GpRoomObjRec D_mine_cavern_80188FE0[];

extern WorldCoordRoomLighting D_mine_cavern_80189010[];

extern u8* D_mine_cavern_80189060[];

extern GpViewCountRec D_mine_cavern_8018906C[];

extern GpWarpRec D_mine_cavern_80189074[];

extern GpViewRec D_mine_cavern_80189840[];

extern SpriteView D_mine_cavern_8018CD10[];

extern WorldCollisionSurfaceProperties* D_mine_cavern_8018E30C[];

void func_mine_cavern_80180320(Task* task);

void func_mine_cavern_8017E474(Task* arg0);

void func_mine_cavern_8017F240(Task* task);

void func_mine_cavern_8017FF88(Task* arg0);

void func_mine_cavern_80181730(Task* arg0);

void func_mine_cavern_8017DF54(Task* task);

#endif // INCLUDE_ROOMS_MINE_CAVERN_H
