#ifndef INCLUDE_ROOMS_SHELTER_1F_GUARDROOM_H
#define INCLUDE_ROOMS_SHELTER_1F_GUARDROOM_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// shelter_1f_guardroom
extern WorldCollisionRoomResources D_shelter_1f_guardroom_8017DA78[];

extern WorldCoordRoomLighting D_shelter_1f_guardroom_8017DA88[];

extern u8* D_shelter_1f_guardroom_8017DA90[];

extern ViewCount D_shelter_1f_guardroom_8017DA94[];

extern GpWarpRec D_shelter_1f_guardroom_8017DA98[];

extern ViewCamera D_shelter_1f_guardroom_8017DC14[];

extern SpriteView D_shelter_1f_guardroom_8017DCE0[];

extern WorldCollisionSurfaceProperties* D_shelter_1f_guardroom_8017DFF4[];

void func_shelter_1f_guardroom_8017D880(Task* task);

void func_shelter_1f_guardroom_8017DA28(Task* unused);

#endif // INCLUDE_ROOMS_SHELTER_1F_GUARDROOM_H
