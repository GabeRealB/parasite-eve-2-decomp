#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_R08_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_R08_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Dryfield night r08 placement/resource directory indexed by `GameLocationKey.variant` 0..10.
///
/// Slot 1 publishes paired tables; all other slots are absent (NULL, NULL).
/// This placement selector is independent of the room and logical-view selectors.
/// Placements belong to the stage map; resource recipes belong to this room.
/// Keep both overlays and the actors supplying task descriptors loaded during
/// loading, spawning and model-flag updates. Lookups require an index within
/// this array; there is no terminator or bounds check.
extern AreaVariant gDryfieldNightR08AreaVariants[11];

/// Room 8's lighting descriptor, selected by room - 1 (room 1 only).
///
/// Borrows three all-view directional lights and uses the default ambient minima.
/// The room overlay and mutable lights must stay loaded while lighting is queried.
extern WorldCoordRoomLighting gDryfieldNightR08RoomLighting[1];

/// Room 8's collision resources, selected by room - 1 (room 1 only).
///
/// Supplies the room mesh without view-boundary triggers, action triggers or
/// sight occluders. All borrowed storage remains live while this overlay is loaded.
extern WorldCollisionRoomResources gDryfieldNightR08RoomCollisionResources[1];

/// Room 8's logical-view maps, selected by room - 1 (room 1 only).
///
/// Borrows gameplay's identity map: logical views 1..9 select matching camera,
/// image and sprite indices. Gameplay and this room overlay must remain loaded.
extern u8* gDryfieldNightR08ViewMaps[1];

/// Room 8's reverse-lookup limit of nine logical views, selected by room - 1.
///
/// The only room slot is room 1. This signed element count limits map searches;
/// it does not describe the shared identity map's full capacity.
extern ViewCount gDryfieldNightR08ViewCounts[1];

extern DirectionWarpEntry D_dryfield_night_r08_8018069C[];

/// Room 8's nine cameras, selected by the mapped 1-based view index minus one.
///
/// Matrices rotate world coordinates with 4096 representing unity and store
/// negated origins in world units; projection distances are pixels. Records
/// must remain loaded until a queued camera application has consumed them.
extern ViewCamera gDryfieldNightR08ViewCameras[9];

/// Room 8's nine sprite descriptors, selected by mapped view index minus one.
///
/// Every view retains decoded background strips; view 6 adds nine sprites.
/// No view supplies draw-area clips. Descriptors and mutable sources/batches
/// borrow the loaded room overlay's lifetime.
extern SpriteView gDryfieldNightR08SpriteViews[9];

/// Room 8's collision and footstep policies, indexed by surface class 0..7.
///
/// Classes 0..4 and 7 block probes and allow weapon impacts; class 5 passes
/// probes and disables those impacts; class 6 blocks and supplies footstep cues.
/// Every class applies contact pushback. Entries borrow this overlay's storage.
extern WorldCollisionSurfaceProperties* gDryfieldNightR08SurfacePropertiesByClass[8];

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

/// Runs room 8's impact flash with smoke or fading rings and bouncing sparks.
///
/// Requires a counted `EffectWork` in `spawnArg2.pointer`, a coordinate body
/// and state/age zero from `effectSpawn`. Nonzero `spawnArg1.value` selects
/// smoke; zero selects rings and sparks. Active age seven enters release and
/// the next active tick frees work and tears down the task. Nonzero room effect
/// control pauses below four and cancels at four or above. The room overlay and any
/// borrowed parent coordinate must remain live through the effect.
void dryfieldNightR08RoomVisualEffectsSparkBurstTask(Task* task);

/// Runs nighttime area 8's room initialization, message wait and teardown.
///
/// State 0 installs the room handlers, selects scene-payload storage and
/// starts the entry script unless demo scene 9 is selected; state 1 waits for
/// messages; state 2 kills the task.
/// Requires a live task with state in 0..2 and the room overlay loaded.
/// The initialized task must stay live while its published room slot is used.
void dryfieldNightR08RoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_R08_H
