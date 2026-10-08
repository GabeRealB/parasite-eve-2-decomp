#ifndef INCLUDE_ROOMS_DRYFIELD_MOTEL_LOFT_H
#define INCLUDE_ROOMS_DRYFIELD_MOTEL_LOFT_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// dryfield_motel_loft
extern WorldCollisionRoomResources D_dryfield_motel_loft_8017D6DC[];

extern u8* D_dryfield_motel_loft_8017D6EC[];

extern ViewCount D_dryfield_motel_loft_8017D6F0[];

extern WorldCoordRoomLighting D_dryfield_motel_loft_8017D6F4[];

extern DirectionWarpEntry D_dryfield_motel_loft_8017D6FC[];

extern ViewCamera D_dryfield_motel_loft_8017D9E0[];

extern SpriteView D_dryfield_motel_loft_8017DDD8[];

extern WorldCollisionSurfaceProperties* D_dryfield_motel_loft_8017E648[];

/// Runs the motel loft's room-message receiver.
///
/// Start the borrowed task in state 0 to install this overlay's message table
/// and register `GAME_TASK_SLOT_ROOM`; state 1 idles and state 2 releases it.
/// The state index must be 0..2; dispatch is unchecked. Keep this overlay and
/// the registered task live while messages can arrive. Registration borrows
/// the task pointer and does not clear the slot on teardown.
void dryfieldMotelLoftTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MOTEL_LOFT_H
