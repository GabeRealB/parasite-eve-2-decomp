#ifndef INCLUDE_ROOMS_MINE_MESA_H
#define INCLUDE_ROOMS_MINE_MESA_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_mine_mesa_801898F4[12];

// mine_mesa
extern WorldCollisionRoomResources D_mine_mesa_80186538[];

extern u8* D_mine_mesa_80186548[];

extern WorldCoordRoomLighting D_mine_mesa_8018654C[];

extern ViewCount D_mine_mesa_80186554[];

extern DirectionWarpEntry D_mine_mesa_80186558[];

extern ViewCamera D_mine_mesa_80187030[];

extern SpriteView D_mine_mesa_80188744[];

extern WorldCollisionSurfaceProperties* D_mine_mesa_80189A60[];

void func_mine_mesa_801811C4(s32 height);

/// Charges a pink flash, tints the screen at its peak, then fades it as a star.
///
/// Requires a counted effect with a coordinate body and owned `EffectWork` in
/// `spawnArg2.pointer`. `spawnArg1.value` is a positive charge duration in
/// ticks, consumed as a countdown. Room effect control pauses it when nonzero
/// and cancels it at four or above; state 3 also releases the work and task.
void mineMesaRoomVisualEffectsFlashTask(Task* task);

/// Draws a blue beam trail between two offsets on the effect's parent coordinate.
///
/// Requires a counted effect's coordinate body and owned `EffectWork` in
/// `spawnArg2.pointer`, with its borrowed parent live throughout the effect.
/// Stores eight world-space snapshots per endpoint in owned `task->work`;
/// task teardown frees that history. `spawnArg1.value` is the active-tick
/// release age (2..32767 before age wraps, 0 indefinite). Initialization uses
/// age 1 without testing expiry. Control values below 2 update the trail;
/// values of 2 or above freeze it. History-allocation failure retries next tick.
void mineMesaRoomVisualEffectsTwinTrailTask(Task* task);

void func_mine_mesa_8018057C(Task* task);

/// Installs Mine Mesa's actor-effect IDs and draws the mapped view's fixed flares.
///
/// Requires a live room-effect controller and current camera/frame state.
/// State 0 installs the flash, twin-trail and spark-burst IDs and enables
/// view effects. Views 2, 4, 5, 6 and 8..11 draw fixed world-point flares;
/// other views draw none. Texture columns 0/1 use radius scales 768/512,
/// projected as scale * 39 / (camera Z / 4) pixels, with nonzero depth required.
void mineMesaDrawViewFlaresTask(Task* task);

/// Task entries the Shelter map UI overlay's stage tables name, each room's
/// entry task started for its location and the enemy descriptors' tasks, and
/// the models those descriptors attach.
void func_mine_mesa_8017DD98(Task* task);

#endif // INCLUDE_ROOMS_MINE_MESA_H
