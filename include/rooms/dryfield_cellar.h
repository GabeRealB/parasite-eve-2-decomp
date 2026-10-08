#ifndef INCLUDE_ROOMS_DRYFIELD_CELLAR_H
#define INCLUDE_ROOMS_DRYFIELD_CELLAR_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_cellar_80180ACC[12];

// dryfield_cellar
extern WorldCollisionRoomResources D_dryfield_cellar_8017DBDC[];

extern u8* D_dryfield_cellar_8017DC04[];

extern ViewCount D_dryfield_cellar_8017DC0C[];

extern WorldCoordRoomLighting D_dryfield_cellar_8017DC10[];

extern DirectionWarpEntry D_dryfield_cellar_8017DC20[];

extern ViewCamera D_dryfield_cellar_8017DF78[];

extern SpriteView D_dryfield_cellar_8017FE40[];

extern WorldCollisionSurfaceProperties* D_dryfield_cellar_80180B40[];

/// Draws the cellar's switch-gated pair of flickering flares each frame.
///
/// `GAME_FLAG_UNDERPASS_SWITCH_2` must equal 1; camera views 2 and 3 select
/// separate pairs of local points, and other views draw nothing. `task` must
/// own a live `TASK_BODY_COORD` body with its cached transform refreshed.
/// The current view, scratch stack, ordering table and packet arena must be
/// ready for drawing. Borrows the coordinate and room's points for this call;
/// queued flare packets belong to the current frame.
void dryfieldCellarDrawGlowsTask(Task* task);

/// Runs the cellar's room receiver through initialization, idle and teardown.
///
/// Requires a live bodyless task with state 0 (register), 1 (idle) or 2 (kill).
/// Initialization registers `GAME_TASK_SLOT_ROOM` and permits post-CAP sound
/// messages. Keep the cellar overlay loaded for the task's lifetime.
void dryfieldCellarRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_CELLAR_H
