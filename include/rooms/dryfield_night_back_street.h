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

/// Night Back Street room 1's six point lights and ambient minima for five views.
///
/// Night Dryfield area 5 indexes this array by room minus one. Light coordinates
/// and attenuation remain writable; ambient colours use 16 units per 8-bit
/// colour level. Borrow these resources only while this room overlay is loaded.
extern WorldCoordRoomLighting gDryfieldNightBackStreetRoomLighting[1];

/// Night Back Street room 1's mapping from logical views to camera/image/sprite slots.
///
/// The sole room entry borrows gameplay's `gViewIdentityMap`: views 1..5 map
/// to slots 1..5. Its capacity is independent of this room's five-view limit.
/// The directory lives with this overlay; the byte map lives with gameplay.
extern u8* gDryfieldNightBackStreetViewMaps[1];

/// Night Back Street room 1's five logical views, bounding reverse-map searches.
///
/// Indexed by room minus one. The signed halfword counts map elements;
/// the stage directory borrows this storage while the room overlay is loaded.
extern ViewCount gDryfieldNightBackStreetRoomViewCounts[1];

extern DirectionWarpEntry D_dryfield_night_back_street_801803CC[];

/// Night Back Street's five mapped-view cameras, indexed by view minus one.
///
/// Rotation uses 4096 per unit, translation is the negated camera origin in
/// game units, and projection distance is pixels. Keep the room overlay loaded
/// until queued camera application has copied the selected record.
extern ViewCamera gDryfieldNightBackStreetViewCameras[5];

/// Night Back Street's five mapped-view sprite descriptors, indexed by view minus one.
///
/// Views 1..3 have empty batches; views 4/5 each select five source rectangles.
/// Every view retains decoded background strips and has no clipping list.
/// Writable descriptors and their lists are borrowed while this overlay is loaded.
extern SpriteView gDryfieldNightBackStreetSpriteViews[5];

/// Night Back Street's collision and footstep policies for surface classes 0..7.
///
/// Class 1 blocks probes and permits weapon impacts with footstep cues; class 2
/// passes probes and suppresses impacts with the same cues. Classes 0 and 3..7
/// block probes and permit impacts without footsteps. All classes apply pushback.
/// The mesh uses classes 0..2; setup caches all eight. Borrowed policies and
/// sounds remain live only while this room overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldNightBackStreetSurfacePropertiesByClass[8];

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

/// Runs the night back street's impact flash with smoke or rings and bouncing sparks.
///
/// Starts in state 0 with a coordinate body and a counted, owned, zero-aged
/// `EffectWork` in `spawnArg2.pointer`. Nonzero `spawnArg1.value` selects smoke;
/// zero selects sparks and orange rings. Active age 7 enters release and age 8
/// frees the work and task. Nonzero room control below 4 pauses it; 4 or above
/// cancels it. Spawned effects run independently; keep their callbacks loaded.
void dryfieldNightBackStreetRoomVisualEffectsSparkBurstTask(Task* task);

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

/// Dispatches the night back street's room receiver lifecycle.
///
/// `state` must be 0 (register messages), 1 (idle), or 2 (kill). Start in state
/// 0; initialization publishes the task in `GAME_TASK_SLOT_ROOM` and enters
/// state 1. The room's borrowed message and state tables must remain loaded
/// through teardown. Dispatch copies the complete three-entry state table.
void dryfieldNightBackStreetRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_BACK_STREET_H
