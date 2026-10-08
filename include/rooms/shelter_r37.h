#ifndef INCLUDE_ROOMS_SHELTER_R37_H
#define INCLUDE_ROOMS_SHELTER_R37_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// shelter_r37
extern u8* D_shelter_r37_8017D6F8[];

extern ViewCount D_shelter_r37_8017D6FC[];

extern DirectionWarpEntry D_shelter_r37_8017D700[];

extern WorldCollisionGrid D_shelter_r37_8017D920;

extern ViewCamera D_shelter_r37_8017D944[];

extern SpriteView D_shelter_r37_8017D9E0[];

extern WorldCoordRoomLights D_shelter_r37_8017DD44;

extern WorldCollisionTrigger D_shelter_r37_8017DD5C[];

extern WorldCollisionSurfaceProperties* D_shelter_r37_8017DED8[];

/// Runs the room controller's initialize, idle or teardown state.
///
/// Requires this overlay loaded and a live bodyless task with state 0..2.
/// State 0 publishes the task in `GAME_TASK_SLOT_ROOM` and installs its message
/// table; state 1 waits without per-frame work, and state 2 releases the task.
/// Allocates no work. The Shelter map's room table starts it in state 0.
void shelterR37RoomTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_R37_H
