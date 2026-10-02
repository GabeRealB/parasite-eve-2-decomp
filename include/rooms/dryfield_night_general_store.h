#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_GENERAL_STORE_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_GENERAL_STORE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_night_general_store_8018572C[22];

// dryfield_night_general_store
extern WorldCoordRoomLighting D_dryfield_night_general_store_8017E82C[];

extern WorldCollisionRoomResources D_dryfield_night_general_store_8017E834[];

extern u8* D_dryfield_night_general_store_8017E844[];

extern ViewCount D_dryfield_night_general_store_8017E848[];

extern DirectionWarpEntry D_dryfield_night_general_store_8017E84C[];

extern ViewCamera D_dryfield_night_general_store_8017F4A8[];

extern SpriteView D_dryfield_night_general_store_80184278[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_general_store_80185894[];

void func_dryfield_night_general_store_8017E6C8(Task* unused);

void func_dryfield_night_general_store_8017DE88(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_GENERAL_STORE_H
