#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_BACK_STREET_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_BACK_STREET_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_back_street_80181518[22];

// dryfield_night_back_street
extern WorldCollisionRoomResources D_dryfield_night_back_street_801803AC[];

extern WorldCoordRoomLighting D_dryfield_night_back_street_801803BC[];

extern u8* D_dryfield_night_back_street_801803C4[];

extern ViewCount D_dryfield_night_back_street_801803C8[];

extern DirectionWarpEntry D_dryfield_night_back_street_801803CC[];

extern ViewCamera D_dryfield_night_back_street_80180B58[];

extern SpriteView D_dryfield_night_back_street_80180D34[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_back_street_8018161C[];

/// Runs the night back street's charging pink flash, peak screen tint and fading star.
///
/// Starts in state 0 with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`. `spawnArg1.value` is a positive
/// charge duration in active ticks, consumed as a countdown. Nonzero room
/// effect control pauses it; values at least four cancel it. State 3 also
/// releases the work and task. The effect controller and room overlay must
/// remain live until teardown.
void dryfieldNightBackStreetRoomVisualEffectsFlashTask(Task* task);

/// Runs the night back street's fading sword beam between two moving endpoint histories.
///
/// Starts in state 0 with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`; its parent coordinate must stay live.
/// Owns two eight-coordinate histories in `Task::work`, freed by task teardown.
/// Allocation failure retries with age zero. Initialization counts as the first
/// active tick; later ticks record endpoints and draw seven quads tinted 1:2:3
/// in R:G:B. `spawnArg1.value` is 0 for external teardown or 2..32767 for an
/// active-age limit. Room effect control at two or above suspends recording and
/// drawing without cancelling. Requires a live effect controller and room overlay.
void dryfieldNightBackStreetRoomVisualEffectsTwinTrailTask(Task* task);

void func_dryfield_night_back_street_8017F6DC(Task* task);

/// Selects the night back street's enemy effects and draws the current view's light glows.
///
/// State 0 installs the room's flash, sword-trail and spark-burst IDs on every
/// tick until another caller changes the state. Every tick enables the room's
/// view-effect gate. View 2 draws two flares and two shafts; view 3 adds two
/// flares to that set; views 4 and 5 share two flares. Other views draw none.
/// Requires live session/effect state, current view transforms, scratch space
/// and frame packet storage. The room overlay must stay loaded while its
/// selected effects can be spawned or run.
void dryfieldNightBackStreetDrawGlowsTask(Task* task);

void func_dryfield_night_back_street_8017D788(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_BACK_STREET_H
