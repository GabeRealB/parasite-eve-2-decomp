#ifndef INCLUDE_ROOMS_DRYFIELD_SOUVENIR_SHOP_H
#define INCLUDE_ROOMS_DRYFIELD_SOUVENIR_SHOP_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_souvenir_shop_8017F598[13];

// dryfield_souvenir_shop
extern GpRoomObjRec D_dryfield_souvenir_shop_8017E0BC[];

extern u8* D_dryfield_souvenir_shop_8017E0CC[];

extern GpViewCountRec D_dryfield_souvenir_shop_8017E0D0[];

extern GpRoomCoordRec D_dryfield_souvenir_shop_8017E0D4[];

extern GpWarpRec D_dryfield_souvenir_shop_8017E0DC[];

extern GpViewRec D_dryfield_souvenir_shop_8017E600[];

extern GpSprtRec D_dryfield_souvenir_shop_8017EED0[];

extern GpRoomParamRec* D_dryfield_souvenir_shop_8017F640[];

void func_dryfield_souvenir_shop_8017DFD4(Task* task);

void func_dryfield_souvenir_shop_8017D65C(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_SOUVENIR_SHOP_H
