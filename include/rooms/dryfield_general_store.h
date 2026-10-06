#ifndef INCLUDE_ROOMS_DRYFIELD_GENERAL_STORE_H
#define INCLUDE_ROOMS_DRYFIELD_GENERAL_STORE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_general_store_80185654[13];

// dryfield_general_store
extern WorldCollisionRoomResources D_dryfield_general_store_8017E670[];

extern u8* D_dryfield_general_store_8017E680[];

extern ViewCount D_dryfield_general_store_8017E684[];

extern WorldCoordRoomLighting D_dryfield_general_store_8017E688[];

extern DirectionWarpEntry D_dryfield_general_store_8017E690[];

extern ViewCamera D_dryfield_general_store_8017F25C[];

extern SpriteView D_dryfield_general_store_8018402C[];

extern WorldCollisionSurfaceProperties* D_dryfield_general_store_801856D8[];

/// Inert callback for the General Store's room-effect slot 0xC4 in task bank 6.
///
/// Ignores `unusedTask`, draws nothing and neither advances nor kills the task.
/// Its coordinate body remains owned by the task until external teardown.
void dryfieldGeneralStoreNoOpEffectTask(Task* unusedTask);

void func_dryfield_general_store_8017DF5C(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_GENERAL_STORE_H
