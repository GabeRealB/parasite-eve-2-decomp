#ifndef INCLUDE_ROOMS_NEO_ARK_R26_H
#define INCLUDE_ROOMS_NEO_ARK_R26_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_neo_ark_r26_8017E994[11];

// neo_ark_r26
extern WorldCollisionRoomResources D_neo_ark_r26_8017E0CC[];

extern WorldCoordRoomLighting D_neo_ark_r26_8017E0DC[];

extern u8* D_neo_ark_r26_8017E0E4[];

extern ViewCount D_neo_ark_r26_8017E0E8[];

extern DirectionWarpEntry D_neo_ark_r26_8017E0EC[];

extern ViewCamera D_neo_ark_r26_8017E1C0[];

extern SpriteView D_neo_ark_r26_8017E898[];

extern WorldCollisionSurfaceProperties* D_neo_ark_r26_8017EA30[];

/// Leaves the room's effect task unchanged each tick.
///
/// Effect bank 6, slot 0x168 supplies a single-coordinate task. This callback
/// ignores it, draws nothing and performs no teardown; the task remains live
/// until another owner tears it down. The `neo_ark_r26` overlay must remain
/// loaded while this callback can be dispatched.
void neoArkR26EffectNoopTask(Task* unusedTask);

/// Runs the Neo Ark room 26 room controller for one tick.
///
/// `task` must be live with state 0..2 and this room overlay loaded.
/// Initialize room messages and the entry scene, remain idle, then kill.
/// State 0 registers the borrowed task in `GAME_TASK_SLOT_ROOM`; state 2
/// requests teardown. The controller allocates no work or body of its own.
void neoArkR26RoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_R26_H
