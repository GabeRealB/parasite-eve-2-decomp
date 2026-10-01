#ifndef INCLUDE_ROOMS_SHELTER_R37_H
#define INCLUDE_ROOMS_SHELTER_R37_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// shelter_r37
extern u8* D_shelter_r37_8017D6F8[];

extern GpViewCountRec D_shelter_r37_8017D6FC[];

extern GpWarpRec D_shelter_r37_8017D700[];

extern WorldCollisionGrid D_shelter_r37_8017D920;

extern GpViewRec D_shelter_r37_8017D944[];

extern GpSprtRec D_shelter_r37_8017D9E0[];

extern WorldCoordRoomLights D_shelter_r37_8017DD44;

extern WorldCollisionTrigger D_shelter_r37_8017DD5C[];

extern WorldCollisionSurfaceProperties* D_shelter_r37_8017DED8[];

void func_shelter_r37_8017D678(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_R37_H
