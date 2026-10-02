#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_SOUVENIR_SHOP_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_SOUVENIR_SHOP_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_night_souvenir_shop_8017F62C[12];

// dryfield_night_souvenir_shop
extern WorldCoordRoomLighting D_dryfield_night_souvenir_shop_8017E0E4[];

extern WorldCollisionRoomResources D_dryfield_night_souvenir_shop_8017E0EC[];

extern u8* D_dryfield_night_souvenir_shop_8017E0FC[];

extern ViewCount D_dryfield_night_souvenir_shop_8017E100[];

extern DirectionWarpEntry D_dryfield_night_souvenir_shop_8017E104[];

extern ViewCamera D_dryfield_night_souvenir_shop_8017E628[];

extern SpriteView D_dryfield_night_souvenir_shop_8017EF08[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_souvenir_shop_8017F6CC[];

void func_dryfield_night_souvenir_shop_8017DFF4(Task* task);

void func_dryfield_night_souvenir_shop_8017D65C(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_SOUVENIR_SHOP_H
