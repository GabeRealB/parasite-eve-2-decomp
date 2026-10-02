#ifndef INCLUDE_ROOMS_SHELTER_R49_H
#define INCLUDE_ROOMS_SHELTER_R49_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_shelter_r49_8017DA00[2];

extern GpAreaVariant D_shelter_r49_8017DD74[13];

// shelter_r49
extern WorldCollisionRoomResources D_shelter_r49_8017DA18[];

extern u8* D_shelter_r49_8017DA28[];

extern ViewCount D_shelter_r49_8017DA2C[];

extern WorldCoordRoomLighting D_shelter_r49_8017DA30[];

extern GpWarpRec D_shelter_r49_8017DA38[];

extern ViewCamera D_shelter_r49_8017DAD0[];

extern SpriteView D_shelter_r49_8017DCA0[];

extern WorldCollisionSurfaceProperties* D_shelter_r49_8017DDF8[];

void func_shelter_r49_8017D6C4(Task* task);

void func_shelter_r49_8017D9D0(Task* unused);

#endif // INCLUDE_ROOMS_SHELTER_R49_H
