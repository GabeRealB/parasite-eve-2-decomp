#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_TOILET_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_TOILET_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_night_toilet_8017F354[12];

// dryfield_night_toilet
extern WorldCoordRoomLighting D_dryfield_night_toilet_8017DAB0[];

extern WorldCollisionRoomResources D_dryfield_night_toilet_8017DAB8[];

extern u8* D_dryfield_night_toilet_8017DAC8[];

extern ViewCount D_dryfield_night_toilet_8017DACC[];

extern DirectionWarpEntry D_dryfield_night_toilet_8017DAD0[];

extern ViewCamera D_dryfield_night_toilet_8017DDAC[];

extern SpriteView D_dryfield_night_toilet_8017EC40[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_toilet_8017F3D8[];

void func_dryfield_night_toilet_8017D9F8(Task* unused);

void func_dryfield_night_toilet_8017D724(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_TOILET_H
