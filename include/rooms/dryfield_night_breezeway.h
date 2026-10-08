#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_BREEZEWAY_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_BREEZEWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_breezeway_801803E4[22];

// dryfield_night_breezeway
extern WorldCoordRoomLighting D_dryfield_night_breezeway_8017E6E4[];

extern WorldCollisionRoomResources D_dryfield_night_breezeway_8017E6EC[];

extern u8* D_dryfield_night_breezeway_8017E6FC[];

extern ViewCount D_dryfield_night_breezeway_8017E700[];

extern DirectionWarpEntry D_dryfield_night_breezeway_8017E704[];

extern ViewCamera D_dryfield_night_breezeway_8017EBE8[];

extern SpriteView D_dryfield_night_breezeway_8017FD10[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_breezeway_801804B8[];

/// Draws the night breezeway's view-dependent light glows.
///
/// Enables room-view effects on every tick. View 2 queues a textured flare and
/// a shaft; view 3 queues an extra shaft before the pulsing red star and shaft
/// shared with view 4. Other views queue nothing. `task` is unused.
///
/// Requires the room overlay, session and room-effect state to remain live,
/// a composed view matrix, an initialized scratch stack and room in the current
/// frame's packet arena and ordering table. Queued packets live until GPU completion.
void dryfieldNightBreezewayDrawGlowsTask(Task* task);

/// Dispatches the night breezeway's room receiver lifecycle.
///
/// `state` must be 0 (register messages), 1 (idle), or 2 (kill). Start in state
/// 0; initialization publishes the task in `GAME_TASK_SLOT_ROOM` and enters
/// state 1. The room's borrowed message and state tables must remain loaded
/// through teardown. Dispatch copies the complete three-entry state table.
void dryfieldNightBreezewayRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_BREEZEWAY_H
