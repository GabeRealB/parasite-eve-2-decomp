#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_JUNK_YARD_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_JUNK_YARD_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_junk_yard_801843F0[22];

// dryfield_night_junk_yard
extern WorldCoordRoomLighting D_dryfield_night_junk_yard_80180784[];

extern WorldCollisionRoomResources D_dryfield_night_junk_yard_80180794[];

extern u8* D_dryfield_night_junk_yard_801807C0[];

extern ViewCount D_dryfield_night_junk_yard_801807C8[];

extern DirectionWarpEntry D_dryfield_night_junk_yard_801807CC[];

extern ViewCamera D_dryfield_night_junk_yard_801811DC[];

extern SpriteView D_dryfield_night_junk_yard_80183700[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_junk_yard_801844C4[];

void func_dryfield_night_junk_yard_8017D9B8(u8 arg0);

/// Runs the junk yard's charging pink flash, peak screen tint and fading star.
///
/// Starts in state 0 with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`. `spawnArg1.value` is a positive
/// charge duration in active ticks, consumed as a countdown. Nonzero room
/// effect control pauses it; values at least four cancel it. State 3 also
/// releases the work and task. Requires live effect state, view transforms,
/// scratch space and frame packet storage; the room overlay must stay loaded.
void dryfieldNightJunkYardRoomVisualEffectsFlashTask(Task* task);

/// Runs the junk yard's fading sword beam between two moving endpoint histories.
///
/// Starts in state 0 with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`; its parent coordinate must stay live.
/// Owns two eight-coordinate histories in `Task::work`, freed by task teardown.
/// Allocation failure retries with age zero. Initialization is the first active
/// tick; subsequent ticks record endpoints and draw seven quads tinted 1:2:3
/// in R:G:B. `spawnArg1.value` is 0 for external teardown or 2..32767 for an
/// active-age limit. Room effect control at two or above suspends recording and
/// drawing without cancelling. Requires live effect state, current view
/// transforms, scratch space, frame packet storage and the loaded room overlay.
void dryfieldNightJunkYardRoomVisualEffectsTwinTrailTask(Task* task);

void func_dryfield_night_junk_yard_8017F914(Task* task);

/// Selects the junk yard's enemy effects and draws the current view's lamp glows.
///
/// State 0 installs the room's flash, sword-trail and spark-burst IDs on every
/// tick until another caller changes the state. Every tick enables the room's
/// view-effect gate. Views 2/8 draw a capsule and four flares; 4/9 draw three
/// flares; 5/10 draw the capsule and three flares; 7 draws it with two flares.
/// The capsule caps start at a quarter-turn angle in views 2/8 and zero in
/// 5/7/10. Other views draw none. Requires live session and effect state,
/// current view transforms, scratch space and frame packet storage. The room
/// overlay must stay loaded while its effects can run.
void dryfieldNightJunkYardDrawGlowsTask(Task* task);

void func_dryfield_night_junk_yard_8017D960(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_JUNK_YARD_H
