#ifndef INCLUDE_ROOMS_NEO_ARK_R26_H
#define INCLUDE_ROOMS_NEO_ARK_R26_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_neo_ark_r26_8017E994[11];

// neo_ark_r26
extern GpRoomObjRec D_neo_ark_r26_8017E0CC[];

extern WorldCoordRoomLighting D_neo_ark_r26_8017E0DC[];

extern u8* D_neo_ark_r26_8017E0E4[];

extern GpViewCountRec D_neo_ark_r26_8017E0E8[];

extern GpWarpRec D_neo_ark_r26_8017E0EC[];

extern ViewCamera D_neo_ark_r26_8017E1C0[];

extern SpriteView D_neo_ark_r26_8017E898[];

extern WorldCollisionSurfaceProperties* D_neo_ark_r26_8017EA30[];

void func_neo_ark_r26_8017D778(Task* unused);

void func_neo_ark_r26_8017D720(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_R26_H
