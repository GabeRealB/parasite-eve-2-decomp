#ifndef INCLUDE_ROOMS_MINE_REFUGE_H
#define INCLUDE_ROOMS_MINE_REFUGE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_mine_refuge_80182A00[11];

// mine_refuge
extern WorldCoordRoomLighting D_mine_refuge_801818F0[];

extern WorldCollisionRoomResources D_mine_refuge_801818F8[];

extern u8* D_mine_refuge_80181908[];

extern ViewCount D_mine_refuge_8018190C[];

extern DirectionWarpEntry D_mine_refuge_80181910[];

extern ViewCamera D_mine_refuge_80181BC8[];

extern SpriteView D_mine_refuge_8018264C[];

extern WorldCollisionSurfaceProperties* D_mine_refuge_80182AB4[];

void func_mine_refuge_8017EA78(Task* task);

/// Draws the Mine Refuge lights selected by the mapped camera view.
///
/// View 2 draws a textured flare and cyan star; view 6 draws a cyan burst.
/// Views 3..5 draw the layered panel glow only while the power-panel flag is 1,
/// with a smaller radius in view 3. Other views queue nothing.
/// The task argument is unused. Requires the room's composed view matrix,
/// current frame packet arena and depth ordering table, and initialized scratch
/// stack. Drawing a panel glow leaves one 16-byte scratch reservation active
/// until the enclosing stack reset. The normal game loop resets it each frame.
void mineRefugeDrawGlowsTask(Task* task);

void func_mine_refuge_8017FFBC(Task* task);

#endif // INCLUDE_ROOMS_MINE_REFUGE_H
