#ifndef INCLUDE_ROOMS_MIST_R18_H
#define INCLUDE_ROOMS_MIST_R18_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_mist_r18_80186BFC[13];

// mist_r18
extern WorldCollisionRoomResources D_mist_r18_8018660C[];

extern u8* D_mist_r18_8018661C[];

extern ViewCount D_mist_r18_80186620[];

extern WorldCoordRoomLighting D_mist_r18_80186624[];

extern DirectionWarpEntry D_mist_r18_8018662C[];

extern ViewCamera D_mist_r18_8018671C[];

extern SpriteView D_mist_r18_80186B60[];

extern WorldCollisionSurfaceProperties* D_mist_r18_80186E70[];

void func_mist_r18_8017ED64(Task* task);

#endif // INCLUDE_ROOMS_MIST_R18_H
