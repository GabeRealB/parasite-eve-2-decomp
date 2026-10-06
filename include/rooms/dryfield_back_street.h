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
/// `spawnArg2.pointer` from `Gp_SpawnEff`. `spawnArg1.value` is a positive
/// charge duration in active ticks, consumed as a countdown. Nonzero room
/// effect control pauses it; values at least four cancel it. State 3 also
/// requests release. Completion frees the effect work and tears down the task.
/// Requires a live `gRoomEffectState` and the room overlay to remain loaded.
void dryfieldBackStreetRoomVisualEffectsFlashTask(Task* task);

/// Runs Back Street's fading sword beam between two moving endpoint histories.
///
/// Starts in state 0 with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `Gp_SpawnEff`. Borrows its parent coordinate while
/// live and owns two eight-coordinate histories in `Task::work`, freed during
/// teardown. Allocation failure retries with age reset to zero. Initialization
/// counts as the first active tick; later ticks record world-space endpoints
/// and draw seven quads with red:green:blue intensities in the ratio 1:2:3.
/// `spawnArg1.value` is zero for external teardown, or 2..32767 to expire at
/// that active age. Room effect control at two or above holds age and drawing
/// without cancelling. Requires a live `gRoomEffectState` and loaded room overlay.
void dryfieldBackStreetRoomVisualEffectsTwinTrailTask(Task* task);

void func_dryfield_back_street_8017ED1C(Task* task);

/// Selects Back Street's enemy effects and enables ambient effects in the current view.
///
/// State 0 installs this room's flash, sword-trail and spark-burst effect IDs
/// and advances to state 1. Every tick enables the view's room-effect gate.
/// Requires a live `gRoomEffectState`; the room overlay must stay loaded while
/// its selected effects can be spawned or run.
void dryfieldBackStreetConfigureEffectsTask(Task* task);

void func_dryfield_back_street_8017D918(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_BACK_STREET_H
