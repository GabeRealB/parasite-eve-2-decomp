#ifndef INCLUDE_ROOMS_NEO_ARK_SHRINE_H
#define INCLUDE_ROOMS_NEO_ARK_SHRINE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_neo_ark_shrine_801866C8[13];

// neo_ark_shrine
extern WorldCollisionRoomResources D_neo_ark_shrine_80182724[];

extern WorldCoordRoomLighting D_neo_ark_shrine_80182784[];

extern u8* D_neo_ark_shrine_801827F0[];

extern ViewCount D_neo_ark_shrine_80182808[];

extern DirectionWarpEntry D_neo_ark_shrine_80182814[];

extern ViewCamera D_neo_ark_shrine_801836BC[];

extern SpriteView D_neo_ark_shrine_80185280[];

extern WorldCollisionSurfaceProperties* D_neo_ark_shrine_80186844[];

void func_neo_ark_shrine_8017F8DC(Task* task);

void func_neo_ark_shrine_8017FEA0(Task* task);

void func_neo_ark_shrine_80180904(Task* task);

void func_neo_ark_shrine_801811EC(Task* task);

void func_neo_ark_shrine_8017D948(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_SHRINE_H
