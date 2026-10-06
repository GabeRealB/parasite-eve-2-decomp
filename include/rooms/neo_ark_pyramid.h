#ifndef INCLUDE_ROOMS_NEO_ARK_PYRAMID_H
#define INCLUDE_ROOMS_NEO_ARK_PYRAMID_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_neo_ark_pyramid_80181728[13];

// neo_ark_pyramid
extern WorldCollisionRoomResources D_neo_ark_pyramid_8017FC28[];

extern WorldCoordRoomLighting D_neo_ark_pyramid_8017FC48[];

extern u8* D_neo_ark_pyramid_8017FC60[];

extern ViewCount D_neo_ark_pyramid_8017FC68[];

extern DirectionWarpEntry D_neo_ark_pyramid_8017FC6C[];

extern ViewCamera D_neo_ark_pyramid_801802E8[];

extern SpriteView D_neo_ark_pyramid_80180E18[];

extern WorldCollisionSurfaceProperties* D_neo_ark_pyramid_80181884[];

/// Runs the pyramid's charging pink flash, peak screen tint and fading star.
///
/// Starts in state 0 with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer`. `spawnArg1.value` must be a positive charge duration in
/// active ticks and is consumed as a countdown. Nonzero room effect control
/// pauses the task; values at least four cancel it. State 3 also releases it.
/// Completion frees the effect work and tears down the task. Requires a live
/// `gRoomEffectState` and the room overlay to remain loaded while running.
void neoArkPyramidRoomVisualEffectsFlashTask(Task* task);

/// Runs the pyramid's fading sword beam between two moving endpoint histories.
///
/// Starts in state 0 with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer`. Borrows the work's parent coordinate for the task's
/// lifetime and owns two eight-coordinate histories in `Task::work`, freed on
/// teardown. Allocation failure resets age to zero and retries. Initialization
/// is the first active tick; subsequent ticks draw seven fading quads at a
/// red:green:blue ratio of 1:2:3. `spawnArg1.value` is zero for external teardown
/// or 2..32767 for expiration at that active age. Room effect control at two
/// or above holds age and drawing without cancelling. Requires a live
/// `gRoomEffectState` and the room overlay to remain loaded while running.
void neoArkPyramidRoomVisualEffectsTwinTrailTask(Task* task);

void func_neo_ark_pyramid_8017EF9C(Task* task);

/// Selects the pyramid's enemy effects and enables the current view's ambient effects.
///
/// State 0 installs this room's flash, sword-trail and spark-burst IDs, enables
/// the view gate and advances to state 1. Later ticks leave the gate unchanged.
/// Requires a live `gRoomEffectState`; the room overlay must stay loaded while
/// its selected effects can be spawned or run.
void neoArkPyramidConfigureEffectsTask(Task* task);

void func_neo_ark_pyramid_8017DB98(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_PYRAMID_H
