#ifndef INCLUDE_ROOMS_MINE_GORGE_H
#define INCLUDE_ROOMS_MINE_GORGE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_mine_gorge_80183544[12];

// mine_gorge
extern WorldCoordRoomLighting D_mine_gorge_8017E7A8[];

extern WorldCollisionRoomResources D_mine_gorge_8017E7B8[];

extern u8* D_mine_gorge_8017E7E4[];

extern ViewCount D_mine_gorge_8017E7EC[];

extern DirectionWarpEntry D_mine_gorge_8017E7F0[];

extern ViewCamera D_mine_gorge_8017FA14[];

extern SpriteView D_mine_gorge_801827F8[];

extern WorldCollisionSurfaceProperties* D_mine_gorge_80183644[];

/// Updates the gorge's ambient-effect gate and draws the current view's fixed flares.
///
/// The loaded gorge must supply a mapped view index in 1..11. Views 2 and 6
/// enable ambient effects; view 1 draws no flares. Requires the current view
/// matrices, initialized scratch stack, ordering table and space for up to five
/// textured quads in the frame's primitive arena. The task argument is unused;
/// this per-frame callback neither advances nor releases the task.
void mineGorgeDrawViewFlaresTask(Task* unusedTask);

/// Runs the gorge's room-message receiver and one-time refuge-story setup.
///
/// Requires the loaded gorge and a live task in state 0 (initialize), 1 (idle)
/// or 2 (release). Borrows the room message table while registered; the body
/// and spawn arguments are unused. Teardown leaves the resident room slot set,
/// so message senders must stop using it before the task is killed.
void mineGorgeRoomTask(Task* task);

#endif // INCLUDE_ROOMS_MINE_GORGE_H
