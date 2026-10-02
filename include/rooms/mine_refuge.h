#ifndef INCLUDE_ROOMS_MINE_REFUGE_H
#define INCLUDE_ROOMS_MINE_REFUGE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_mine_refuge_80182A00[11];

// mine_refuge
extern WorldCoordRoomLighting D_mine_refuge_801818F0[];

extern WorldCollisionRoomResources D_mine_refuge_801818F8[];

extern u8* D_mine_refuge_80181908[];

extern ViewCount D_mine_refuge_8018190C[];

extern DirectionWarpEntry D_mine_refuge_80181910[];

extern ViewCamera D_mine_refuge_80181BC8[];

extern SpriteView D_mine_refuge_8018264C[];

extern WorldCollisionSurfaceProperties* D_mine_refuge_80182AB4[];

void func_mine_refuge_8017EA78(Task* task);

void func_mine_refuge_80181454(Task* unused);

void func_mine_refuge_8017FFBC(Task* task);

#endif // INCLUDE_ROOMS_MINE_REFUGE_H
