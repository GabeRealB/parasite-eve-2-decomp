#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_CELLAR_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_CELLAR_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_cellar_80180780[12];

// dryfield_night_cellar
extern WorldCoordRoomLighting D_dryfield_night_cellar_8017DAF0[];

extern WorldCollisionRoomResources D_dryfield_night_cellar_8017DB00[];

extern u8* D_dryfield_night_cellar_8017DB28[];

extern ViewCount D_dryfield_night_cellar_8017DB30[];

extern DirectionWarpEntry D_dryfield_night_cellar_8017DB34[];

extern ViewCamera D_dryfield_night_cellar_8017DE84[];

extern SpriteView D_dryfield_night_cellar_8017FAB8[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_cellar_801807F4[];

void func_dryfield_night_cellar_8017DA28(Task* unused);

void func_dryfield_night_cellar_8017D748(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_CELLAR_H
