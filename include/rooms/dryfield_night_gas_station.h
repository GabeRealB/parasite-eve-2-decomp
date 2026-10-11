#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_GAS_STATION_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_GAS_STATION_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Dryfield night gas station placement/resource directory indexed by `GameLocationKey.variant` 0..11.
///
/// Slots 1, 2, 3, 7, 11 publish paired tables; all other slots are absent (NULL, NULL).
/// This placement selector is independent of the room and logical-view selectors.
/// Placements belong to the stage map; resource recipes belong to this room.
/// Keep both overlays and the actors supplying task descriptors loaded during
/// loading, spawning and model-flag updates. Lookups require an index within
/// this array; there is no terminator or bounds check.
extern AreaVariant gDryfieldNightGasStationAreaVariants[12];

/// Lighting for night gas station room variants 1..4, indexed by room - 1.
///
/// Room 1 has eight point lights; rooms 2..4 share seven. All variants share
/// ambient minima for logical views 1..21. Borrowed records and light arrays
/// remain valid while this room overlay is loaded; light transforms are writable.
extern WorldCoordRoomLighting gDryfieldNightGasStationRoomLighting[4];

/// Collision resources for night gas station room variants 1..4, indexed by room - 1.
///
/// Rooms 1..3 share the live grid with its restorable cutscene obstacle;
/// room 4 selects a separate mesh. All share view-boundary and interaction
/// triggers and have no sight occluders. The map overlay borrows this directory
/// while the room overlay and its mutable grids/trigger pools remain loaded.
extern WorldCollisionRoomResources gDryfieldNightGasStationRoomCollisionResources[4];

/// Logical-view maps for night gas station room variants 1..4, indexed by room - 1.
///
/// Each map's first 21 bytes select 1-based camera, image and sprite indices
/// for logical views 1..21. Mappings may alias. Maps belong to the loaded
/// room overlay and must remain live while the map and view systems use them.
extern u8* gDryfieldNightGasStationViewMaps[4];

/// Logical-view search limits for room variants 1..4, indexed by room - 1.
///
/// Each room has 21 logical views, independently of aliases in its view map.
/// These signed element counts have no terminator and belong to the loaded
/// room overlay; they do not count the maps' retained trailing bytes.
extern ViewCount gDryfieldNightGasStationViewCounts[4];

/// Three gas-station arrival/departure endpoints, indexed by warp ID minus one.
///
/// IDs 1..3 have initial views 2, 4 and 6; the two action-trigger exits use
/// current endpoint IDs 3 and 2. Positions use room-space game units and yaw
/// uses 1/4096 turns. There is no terminator. Stage-map and direction lookups
/// borrow the table only while the owning room overlay remains loaded.
extern DirectionWarpEntry gDryfieldNightGasStationWarpEntries[3];

/// Cameras shared by all four room variants, indexed by mapped view minus one.
///
/// Mapped indices are 1..21. Rotation uses ONE (4096) for unit scale;
/// translation is the negated world-space camera origin, and perspective
/// distance is in pixels. The table has no terminator. The map and view systems
/// borrow it while this room overlay is loaded.
extern ViewCamera gDryfieldNightGasStationViewCameras[21];

/// Background sprite resources shared by the four rooms, indexed by mapped view minus one.
///
/// Mapped indices are 1..21; there is no descriptor terminator or clipping list.
/// Every view's first zero-count batch selects decoded background image strips.
/// Empty views retain their batch-list address in the source slot without
/// dereferencing it as sprites. Sources and batches belong to the loaded room
/// overlay; cutscene effects and saved balcony damage change batch visibility.
extern SpriteView gDryfieldNightGasStationSpriteViews[21];

/// Collision policies and footstep cue sets indexed by surface class 0..7.
///
/// Classes 0 and 5..7 share a silent blocking record. Class 1 passes probes
/// and suppresses weapon impacts; classes 2..4 block and allow impacts.
/// Every class applies pushback. Classes 1 and 4 share footstep cues, while
/// 2 and 3 use separate pairs. Records and cues belong to the loaded room overlay.
extern WorldCollisionSurfaceProperties* gDryfieldNightGasStationSurfacePropertiesByClass[8];

/// Runs the night gas station's charging pink flash, peak screen tint and fading star.
///
/// Starts in state 0 with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`. `spawnArg1.value` is a positive
/// charge duration in active ticks, consumed as a countdown. Nonzero room
/// effect control pauses it; values at least four cancel it. State 3 also
/// releases the work and task. Requires a live effect controller and room
/// overlay until teardown.
void dryfieldNightGasStationRoomVisualEffectsFlashTask(Task* task);

/// Runs the night gas station's fading sword beam between two moving endpoint histories.
///
/// Starts in state 0 with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`; its parent coordinate must stay live.
/// Owns two eight-coordinate histories in `Task::work`, freed by task teardown.
/// Allocation failure retries with age zero. Initialization counts as the first
/// active tick; later ticks record endpoints and draw seven quads tinted 1:2:3
/// in R:G:B. `spawnArg1.value` is 0 for external teardown or 2..32767 for an
/// active-age limit. Room effect control at two or above suspends recording and
/// drawing without cancelling. Requires a live effect controller and room overlay.
void dryfieldNightGasStationRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs the night gas station's impact flash with smoke or orange rings and bouncing sparks.
///
/// Starts in state zero with a coordinate body and zero-aged, counted
/// `EffectWork` in `spawnArg2.pointer` from `effectSpawn`. Nonzero
/// `spawnArg1.value` selects smoke; zero selects the rings and sparks.
/// Ages 1..7 act, and age 8 releases the work and task. Room effect control
/// pauses at nonzero values below four and cancels at four or above. Child
/// effects own their lifetimes; the room overlay must remain loaded.
void dryfieldNightGasStationRoomVisualEffectsSparkBurstTask(Task* task);

/// Draws the room's view-selected light glows and scatters fire and smoke effects.
///
/// Requires a coordinate body and counted `EffectWork` in `spawnArg2.pointer`
/// from `effectSpawn`, retained until normal task teardown. State zero installs
/// the room's flash, twin-trail and spark-burst IDs and enables view effects;
/// this task does not advance its own state. The mapped view must be 0..20.
/// Capsule pairs and flares use world points; spawn offsets use the task's
/// coordinate frame, with independent X/Z jitter in -255..256 units.
///
/// Initial gas-station progress latches `EffectWork::scale`. Outside battle
/// and with running effect control, each scatter anchor chooses
/// `EFFECT_ADDITIVE_PUFF`, `EFFECT_FIRE_BURST`, or a further one-in-three
/// `EFFECT_SMOKE_PUFF`. Later progress emits smoke only, one-in-three per
/// anchor, and only if this task previously latched initial progress. Lights
/// continue drawing independently of these spawn gates.
void dryfieldNightGasStationAmbientEffectsTask(Task* task);

/// Updates the night gas station's telephone save and statistics menu.
///
/// The task owns a live `UiObject` in `spawnArg2.pointer` and starts in state
/// zero. A prior clear or attract-demo scene 1 enables statistics; otherwise
/// it opens the save dialog directly. Child panels supply the save result and
/// notices, while this callback publishes the menu result and UI-open state.
/// The room overlay and UI task tree must remain live while the menu runs.
void dryfieldNightGasStationTelephoneMenuTask(Task* task);

/// Task entries the Dryfield-at-night map UI overlay's stage tables name, each
/// room's entry task started for its location, and the models its enemy
/// descriptors attach.
/// Dispatches the night gas station's room receiver and deferred scene updates.
///
/// Requires a live bodyless task in state 0..2: initialize, update, release.
/// Restores saved balcony sprite visibility before every state call. The map
/// overlay and this room's callbacks and sprite tables must stay loaded. The
/// registered room-task slot borrows the task and is not cleared on teardown.
void dryfieldNightGasStationRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_GAS_STATION_H
