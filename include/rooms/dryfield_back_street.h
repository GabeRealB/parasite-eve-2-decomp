#ifndef INCLUDE_ROOMS_DRYFIELD_BACK_STREET_H
#define INCLUDE_ROOMS_DRYFIELD_BACK_STREET_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_back_street_80180A50[13];

// dryfield_back_street
extern WorldCollisionRoomResources D_dryfield_back_street_8017F9B4[];

extern u8* D_dryfield_back_street_8017F9C4[];

extern ViewCount D_dryfield_back_street_8017F9C8[];

extern WorldCoordRoomLighting D_dryfield_back_street_8017F9CC[];

extern DirectionWarpEntry D_dryfield_back_street_8017F9D4[];

extern ViewCamera D_dryfield_back_street_801802A8[];

extern SpriteView D_dryfield_back_street_80180484[];

extern WorldCollisionSurfaceProperties* D_dryfield_back_street_80181034[];

/// Runs Back Street's charging pink flash, peak screen tint and fading star.
///
/// Starts in state 0 with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`. `spawnArg1.value` is a positive
/// charge duration in active ticks, consumed as a countdown. Nonzero room
/// effect control pauses it; values at least four cancel it. State 3 also
/// requests release. Completion frees the effect work and tears down the task.
/// Requires a live `gRoomEffectState` and the room overlay to remain loaded.
void dryfieldBackStreetRoomVisualEffectsFlashTask(Task* task);

/// Runs Back Street's fading sword beam between two moving endpoint histories.
///
/// Starts in state 0 with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`. Borrows its parent coordinate while
/// live and owns two eight-coordinate histories in `Task::work`, freed during
/// teardown. Allocation failure retries with age reset to zero. Initialization
/// counts as the first active tick; later ticks record world-space endpoints
/// and draw seven quads with red:green:blue intensities in the ratio 1:2:3.
/// `spawnArg1.value` is zero for external teardown, or 2..32767 to expire at
/// that active age. Room effect control at two or above holds age and drawing
/// without cancelling. Requires a live `gRoomEffectState` and loaded room overlay.
void dryfieldBackStreetRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs Back Street's impact flash followed by smoke or orange rings and sparks.
///
/// Starts in state 0 with a coordinate body and zero-aged counted `EffectWork`
/// in `spawnArg2.pointer` from `effectSpawn`. Nonzero `spawnArg1.value` selects
/// smoke; zero selects rings and two independent bouncing sparks. Enters
/// release at active age seven and frees work on the next active tick. Nonzero
/// room control pauses below four and cancels at four or above. Requires live
/// room-effect state and the loaded room overlay through teardown.
void dryfieldBackStreetRoomVisualEffectsSparkBurstTask(Task* task);

/// Selects Back Street's enemy effects and enables ambient effects in the current view.
///
/// State 0 installs this room's flash, sword-trail and spark-burst effect IDs
/// and advances to state 1. Every tick enables the view's room-effect gate.
/// Requires a live `gRoomEffectState`; the room overlay must stay loaded while
/// its selected effects can be spawned or run.
void dryfieldBackStreetConfigureEffectsTask(Task* task);

/// Runs Back Street's room-message receiver and starts its ambience.
///
/// State 0 registers the receiver and spawns the ambience task; state 1
/// idles with messages available; state 2 releases the task.
/// Start with a live bodyless task in state 0. The state must remain in 0..2;
/// dispatch performs no bounds check. Keep the room overlay and gameplay
/// resources loaded through the selected handler, which may release the task.
void dryfieldBackStreetRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_BACK_STREET_H
