#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_R08_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_R08_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_r08_801818D8[11];

// dryfield_night_r08
extern WorldCoordRoomLighting D_dryfield_night_r08_8018067C[];

extern WorldCollisionRoomResources D_dryfield_night_r08_80180684[];

extern u8* D_dryfield_night_r08_80180694[];

extern ViewCount D_dryfield_night_r08_80180698[];

extern DirectionWarpEntry D_dryfield_night_r08_8018069C[];

extern ViewCamera D_dryfield_night_r08_80181498[];

extern SpriteView D_dryfield_night_r08_80181728[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_r08_8018195C[];

/// Selects night room 8's enemy effects and draws the current view's beams and flares.
///
/// State 0 installs the room's flash, sword-trail and spark-burst IDs, then
/// advances to state 1. Every tick draws the placements for views 2..9;
/// other views emit nothing. Requires a live session, composed view matrices,
/// scratch space and frame packet storage. The room overlay must stay loaded
/// while the task runs or its selected enemy effects can be spawned.
void dryfieldNightR08DrawGlowsTask(Task* task);

/// Runs night room 8's charging pink flash, peak screen tint and fading star.
///
/// Starts in state 0 with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`. `spawnArg1.value` is a positive
/// charge duration in active ticks, consumed as a countdown. Nonzero room
/// effect control pauses it; values at least four cancel it. State 3 also
/// releases the work and task. Requires a live effect controller and room overlay.
void dryfieldNightR08RoomVisualEffectsFlashTask(Task* task);

/// Runs night room 8's fading sword trail between two moving endpoint histories.
///
/// Starts in state 0 with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`; its parent coordinate must stay live.
/// Owns two eight-coordinate histories in `Task::work`, freed by task teardown.
/// Allocation failure retries with age zero. Initialization counts as the first
/// active tick; later ticks record endpoints and draw seven quads tinted 1:2:3
/// in R:G:B. `spawnArg1.value` is 0 for external teardown or 2..32767 for an
/// active-age limit. Room effect control at two or above suspends recording and
/// drawing without cancelling. Requires a live effect controller and room overlay.
void dryfieldNightR08RoomVisualEffectsTwinTrailTask(Task* task);

void func_dryfield_night_r08_8017F8FC(Task* task);

/// Runs nighttime area 8's room initialization, message wait and teardown.
///
/// State 0 installs the room handlers, selects scene-payload storage and
/// starts the entry script unless demo scene 9 is selected; state 1 waits for
/// messages; state 2 kills the task.
/// Requires a live task with state in 0..2 and the room overlay loaded.
/// The initialized task must stay live while its published room slot is used.
void dryfieldNightR08RoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_R08_H
