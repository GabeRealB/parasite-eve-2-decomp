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

/// Lighting for the pyramid's two room variants, indexed by room - 1.
///
/// Both variants borrow the same mutable point lights and use the default
/// ambient minima. The descriptors and lights stay valid while this overlay
/// is loaded.
extern WorldCoordRoomLighting gNeoArkPyramidRoomLighting[2];

/// Logical-view maps for the pyramid's two room variants, indexed by room - 1.
///
/// Each map has eight unsigned bytes indexed by logical view - 1, containing
/// 1-based camera, image and sprite indices. Room 1 uses the identity map;
/// room 2 exchanges indices 4 and 6 after the altar's second sequence is solved.
/// The map directory and room-owned map require this overlay to remain loaded.
extern u8* gNeoArkPyramidViewMaps[2];

extern ViewCount D_neo_ark_pyramid_8017FC68[];

extern DirectionWarpEntry D_neo_ark_pyramid_8017FC6C[];

/// Authored cameras for mapped view indices 1..8, stored at index minus one.
///
/// Rotation coefficients use `ONE` (4096) per unit; translation is the negated
/// world-space camera origin in game units. Projection distance is in pixels.
/// The stage map borrows this array; keep the room overlay loaded until queued
/// camera tasks have applied their records. There is no terminator.
/// Both room maps select this array; the solved-altar map swaps indices 4 and 6.
extern ViewCamera gNeoArkPyramidViewCameras[8];

/// Sprite sources and batch lists for the pyramid's eight mapped views.
///
/// Indexed by mapped view - 1, independently of the logical-view numbering.
/// Views 3..6 have source rectangles; views 1, 2, 7 and 8 have empty batches.
/// Every view retains decoded background strips and has no draw-area list.
/// These mutable descriptors and their lists require this overlay to stay loaded.
extern SpriteView gNeoArkPyramidSpriteViews[8];

/// Collision and footstep policies indexed by the pyramid's surface class (0..7).
///
/// Class 2 passes probes and suppresses weapon impacts; every class applies
/// pushback. Class 3 has walking/running cues and class 4 has stair cues.
/// Classes 0, 5, 6 and 7 share one silent blocking record; class 1 has its own
/// silent blocking record. The directory and records live in this room overlay.
extern WorldCollisionSurfaceProperties* gNeoArkPyramidSurfaceProperties[8];

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

/// Runs the pyramid's impact flash with smoke or orange rings and bouncing sparks.
///
/// Requires state 0, a coordinate body and counted, owned, zero-aged `EffectWork`
/// in `spawnArg2.pointer`; its parent coordinate must stay live until teardown.
/// Nonzero `spawnArg1.value` selects smoke puffs, zero selects two independent
/// sparks followed by fading rings. Active age seven enters release; the next
/// active tick frees the work and task. Nonzero room effect control below four
/// pauses the task; four or above cancels it. Child effects run independently.
/// The gameplay bank-6 descriptor selects this room's loaded implementation.
void neoArkPyramidRoomVisualEffectsSparkBurstTask(Task* task);

/// Selects the pyramid's enemy effects and enables the current view's ambient effects.
///
/// State 0 installs this room's flash, sword-trail and spark-burst IDs, enables
/// the view gate and advances to state 1. Later ticks leave the gate unchanged.
/// Requires a live `gRoomEffectState`; the room overlay must stay loaded while
/// its selected effects can be spawned or run.
void neoArkPyramidConfigureEffectsTask(Task* task);

/// Runs the Neo Ark Pyramid room controller for one tick.
///
/// `task` must be live with state 0..2 and this room overlay loaded.
/// Initialize room messages, draw the rotation puzzle, then kill.
/// State 0 registers the borrowed task in `GAME_TASK_SLOT_ROOM`; state 2
/// requests teardown. The controller allocates no work or body of its own.
void neoArkPyramidRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_PYRAMID_H
