#ifndef INCLUDE_ROOMS_SHELTER_1F_TENT_H
#define INCLUDE_ROOMS_SHELTER_1F_TENT_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_1f_tent_80184230[13];

// shelter_1f_tent
extern u8* D_shelter_1f_tent_80181D44[];

extern ViewCount D_shelter_1f_tent_80181D48[];

extern DirectionWarpEntry D_shelter_1f_tent_80181D4C[];

extern WorldCollisionGrid D_shelter_1f_tent_801822F0;

extern ViewCamera D_shelter_1f_tent_80182314[];

extern SpriteView D_shelter_1f_tent_801838E4[];

extern WorldCoordRoomLights D_shelter_1f_tent_80183A7C;

extern WorldCollisionTrigger D_shelter_1f_tent_80183A94[];

extern WorldCollisionTrigger D_shelter_1f_tent_80183CF4[];

extern WorldCollisionSurfaceProperties* D_shelter_1f_tent_801842B4[];

/// Runs the tent's room controller for one tick.
///
/// Requires a live bodyless task with state 0..2 and loaded room/actor resources.
/// State 0 installs room messages and applies first-arrival or ambience setup;
/// state 1 stays idle for messages and state 2 requests teardown. The controller
/// owns no work; the first-arrival script and scene tasks have their own lifetimes.
void shelter1fTentRoomTask(Task* task);

void func_shelter_1f_tent_8017EA60(Task* task);

/// Draws the tent's fixed world-space glows for the current mapped camera view.
///
/// Views 2..7 select capsules, flickering discs and pulsing cyan highlights;
/// other views emit no packets. `unusedTask` is unused, and no task state changes.
/// Requires the loaded room's view transform, initialized scratch stack and
/// current frame's ordering table and packet arena. Queued packets live until
/// that frame's GPU work completes.
void shelter1fTentDrawViewGlowsTask(Task* unusedTask);

#endif // INCLUDE_ROOMS_SHELTER_1F_TENT_H
