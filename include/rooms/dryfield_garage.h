#ifndef INCLUDE_ROOMS_DRYFIELD_GARAGE_H
#define INCLUDE_ROOMS_DRYFIELD_GARAGE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaApplyRec D_dryfield_garage_80180204[6];

extern SVECTOR gDryfieldGarageCollision0108CNormals[39];

extern SVECTOR gDryfieldGarageCollision0108CVerts[106];

extern WorldCollisionGridFace gDryfieldGarageCollision0108CFaces[54];

extern AreaVariant D_dryfield_garage_801800E0[13];

// dryfield_garage
extern WorldCollisionRoomResources D_dryfield_garage_8017DCDC[];

extern u8* D_dryfield_garage_8017DCEC[];

extern ViewCount D_dryfield_garage_8017DCF0[];

extern WorldCoordRoomLighting D_dryfield_garage_8017DCF4[];

extern DirectionWarpEntry D_dryfield_garage_8017DCFC[];

extern ViewCamera D_dryfield_garage_8017E670[];

extern SpriteView D_dryfield_garage_8017F5E8[];

extern WorldCollisionSurfaceProperties* D_dryfield_garage_801801E4[];

/// Inert room-effect callback for bank 6, slot 0xD6, selected for daytime Garage.
///
/// Ignores `unusedTask` without drawing, advancing state or releasing resources.
/// Keep the Dryfield Garage overlay loaded while this callback is scheduled.
void dryfieldGarageEffectNoopTaskD6(Task* unusedTask);

/// Runs the garage's room receiver through entry setup, idle and teardown.
///
/// Requires a live bodyless task with state 0 (entry), 1 (idle) or 2 (kill).
/// Entry registers `GAME_TASK_SLOT_ROOM`, applies companion placement for warp 2,
/// advances follow-up dialogue and restricts the first action trigger to variant 1.
/// Keep the garage overlay and active layout live for the task's lifetime.
void dryfieldGarageRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_GARAGE_H
