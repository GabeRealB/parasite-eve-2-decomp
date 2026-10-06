#ifndef INCLUDE_ROOMS_DRYFIELD_DRIVEWAY_H
#define INCLUDE_ROOMS_DRYFIELD_DRIVEWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_driveway_8017EDD4[11];

// dryfield_driveway
extern WorldCollisionRoomResources D_dryfield_driveway_8017E784[];

extern u8* D_dryfield_driveway_8017E7AC[];

extern WorldCoordRoomLighting D_dryfield_driveway_8017E7B4[];

extern ViewCount D_dryfield_driveway_8017E7C4[];

extern DirectionWarpEntry D_dryfield_driveway_8017E7C8[];

extern ViewCamera D_dryfield_driveway_8017EE2C[];

extern SpriteView D_dryfield_driveway_8017FC44[];

extern WorldCollisionSurfaceProperties* D_dryfield_driveway_80180660[];

/// Enables the current view's ambient effects on every room-effect update.
///
/// Requires the initialized room-effect controller at `gRoomEffectState`.
/// The task argument is ignored; the callback neither advances nor ends it.
void dryfieldDrivewayEnableAmbientEffectsTask(Task* unusedTask);

void func_dryfield_driveway_8017DE14(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_DRIVEWAY_H
