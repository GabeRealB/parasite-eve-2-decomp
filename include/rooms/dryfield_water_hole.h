#ifndef INCLUDE_ROOMS_DRYFIELD_WATER_HOLE_H
#define INCLUDE_ROOMS_DRYFIELD_WATER_HOLE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_water_hole_801827BC[13];

// dryfield_water_hole
extern WorldCollisionRoomResources D_dryfield_water_hole_8017FD2C[];

extern u8* D_dryfield_water_hole_8017FD84[];

extern ViewCount D_dryfield_water_hole_8017FD94[];

extern WorldCoordRoomLighting D_dryfield_water_hole_8017FD9C[];

extern DirectionWarpEntry D_dryfield_water_hole_8017FDBC[];

extern ViewCamera D_dryfield_water_hole_80180284[];

extern SpriteView D_dryfield_water_hole_80181634[];

extern WorldCollisionSurfaceProperties* D_dryfield_water_hole_801828AC[];

/// Advances and draws one expanding, fading water-surface ripple in this room.
///
/// `task` must be a live counted effect with one coordinate body and an owned,
/// zero-initialized `EffectWork` in `spawnArg2.pointer`, initially in state 0.
/// `spawnArg1` bits 0..11 give the initial local half-side in game coordinate
/// units (0..4095); higher bits are ignored. The room overlay must remain loaded
/// throughout the effect's lifetime.
///
/// Each running update grows the half-side by 32 and dims RGB brightness by 2,
/// drawing 32 times from brightness 64 down to 2. The first update seeds a
/// random yaw at 4096 units per turn; it is composed on the next running update.
/// Non-running controls redraw the retained values without advancing them.
/// Cancellation draws once before teardown; cancellation and fade completion
/// free the work, decrement the live-effect count and release the task/body.
void dryfieldWaterHoleWaterRippleTask(Task* task);

void func_dryfield_water_hole_8017F118(Task* task);

void func_dryfield_water_hole_8017E040(Task* arg0);

void func_dryfield_water_hole_8017D840(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_WATER_HOLE_H
