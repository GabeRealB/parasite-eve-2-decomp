#ifndef INCLUDE_ROOMS_SHELTER_1F_TENT_H
#define INCLUDE_ROOMS_SHELTER_1F_TENT_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_shelter_1f_tent_80184230[13];

// shelter_1f_tent
extern u8* D_shelter_1f_tent_80181D44[];

extern GpViewCountRec D_shelter_1f_tent_80181D48[];

extern GpWarpRec D_shelter_1f_tent_80181D4C[];

extern WorldCollisionGrid D_shelter_1f_tent_801822F0;

extern GpViewRec D_shelter_1f_tent_80182314[];

extern GpSprtRec D_shelter_1f_tent_801838E4[];

extern WorldCoordRoomLights D_shelter_1f_tent_80183A7C;

extern WorldCollisionTrigger D_shelter_1f_tent_80183A94[];

extern WorldCollisionTrigger D_shelter_1f_tent_80183CF4[];

extern WorldCollisionSurfaceProperties* D_shelter_1f_tent_801842B4[];

void func_shelter_1f_tent_8017FDB8(Task* task);

void func_shelter_1f_tent_8017EA60(Task* task);

void func_shelter_1f_tent_8017FE10(Task* unused);

#endif // INCLUDE_ROOMS_SHELTER_1F_TENT_H
