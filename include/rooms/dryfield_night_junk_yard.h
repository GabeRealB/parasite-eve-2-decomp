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

/// Shows or hides the junk-yard encounter's sprite batch in mapped view 7.
///
/// Zero shows the nineteen sprites in batch 5; any nonzero byte hides them.
/// Requires the active location to select the loaded night junk-yard sprite
/// directory and its mutable batch list. Does not change the saved event flag.
void dryfieldNightJunkYardSetEventSpriteBatchHidden(u8 hidden);

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

/// Runs the junk yard's impact flash with smoke or orange rings and bouncing sparks.
///
/// Starts in state 0 with a coordinate body and zero-aged counted `EffectWork`
/// in `spawnArg2.pointer`. Nonzero `spawnArg1.value` selects smoke; zero selects
/// rings and sparks. Active age seven enters release; the next active tick
/// frees the work and task. Nonzero room effect control below four pauses it,
/// and four or above cancels it. Child effects live independently. Requires
/// live effect state, view transforms, scratch and packet storage, and the
/// room overlay loaded until teardown.
void dryfieldNightJunkYardRoomVisualEffectsSparkBurstTask(Task* task);

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

/// Dispatches initialization, idle message reception and teardown for the room.
///
/// Starts in state 0; state must remain in 0..2. Publishes the room task slot
/// and message table on initialization. The room overlay and sprite directory
/// must stay loaded while this task or its message handlers can run.
void dryfieldNightJunkYardRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_JUNK_YARD_H
