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

/// Runs the refuge telephone's save menu and unlocked statistics panels.
///
/// Called each tick by the saved-area telephone dispatcher with the loaded
/// refuge overlay. Borrows the live `UiObject` in `task->spawnArg2` and owns
/// its child dialogs; their storage must remain live through dismissal.
/// Before a clear, normal play enters the save dialog directly. A cleared game
/// or statistics demo shows Save, Play Data, Weapon Data and PE Data choices.
void mineRefugeTelephoneMenuTask(Task* task);

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

/// Dispatches the refuge room task's initialization, idle and release states.
///
/// `task->state` must be 0..2. Initialization installs the room message table,
/// publishes the room task and enables CAP completion sound cues; idle keeps
/// those handlers available, and state 2 releases the task. The refuge overlay
/// must remain loaded while the task or its installed callbacks can run.
void mineRefugeRoomTask(Task* task);

#endif // INCLUDE_ROOMS_MINE_REFUGE_H
