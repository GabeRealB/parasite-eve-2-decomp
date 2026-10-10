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

/// Daytime Back Street room 1's collision mesh, view boundaries and action triggers.
///
/// Dryfield area 5 borrows this room-indexed array at room minus one. Setup
/// binds the grid and links both trigger lists to the current view; there are
/// no sight occluders. Keep the overlay loaded until its resources are unlinked.
extern WorldCollisionRoomResources gDryfieldBackStreetRoomCollisionResources[1];

/// Daytime Back Street room 1's logical-view mapping to camera, image and sprite slots.
///
/// The sole room entry borrows gameplay's `gViewIdentityMap`: logical views
/// 1..5 map to slots 1..5. The room directory lives with this overlay; the
/// shared byte map lives with gameplay and its capacity is independent of
/// this room's five-view reverse-search limit.
extern u8* gDryfieldBackStreetViewMaps[1];

/// Daytime Back Street room 1's five logical views, used to bound reverse mapping.
///
/// Indexed by room minus one; the signed halfword is an element count.
/// This overlay must remain loaded while the stage directory borrows it.
extern ViewCount gDryfieldBackStreetRoomViewCounts[1];

/// Daytime Back Street room 1's point-light collection and default ambient minima.
///
/// Indexed by room minus one. The fourteen white point lights contribute in every
/// view; no room ambient table overrides the gameplay defaults. Coordinates
/// and attenuation remain writable while this overlay is loaded.
extern WorldCoordRoomLighting gDryfieldBackStreetRoomLighting[1];

/// Daytime Back Street's four arrival and departure endpoints.
///
/// One-based slots 1..4 serve the general store, souvenir shop, warehouse and
/// dilapidated house, with initial views 2, 2, 3 and 5. Poses use integer
/// world units and 4096 yaw units per turn. Each endpoint supplies its own
/// sounds and map-marker flag. Borrow only while this overlay is loaded.
extern DirectionWarpEntry gDryfieldBackStreetWarpEntries[4];

/// Daytime Back Street's five mapped-view camera records, indexed by view minus one.
///
/// Rotation coefficients have 12 fractional bits, translations are negated
/// camera origins in world units, and projection distances are pixels.
/// Queued camera application requires this overlay to remain loaded.
extern ViewCamera gDryfieldBackStreetViewCameras[5];

/// Daytime Back Street's five mapped-view sprite descriptors, indexed by view minus one.
///
/// Views 1..3 have empty batches; views 4/5 each select five source rectangles.
/// Every view retains decoded background strips and has no draw-area clipping.
/// The writable descriptors and their lists are borrowed while the overlay is loaded.
extern SpriteView gDryfieldBackStreetSpriteViews[5];

/// Daytime Back Street's collision and footstep policies for surface classes 0..7.
///
/// Class 1 blocks probes and permits weapon impacts with footstep cues; class
/// 2 passes probes and suppresses weapon impacts with the same cues. Classes
/// 0 and 3..7 block probes and permit impacts without footsteps. Every class
/// applies pushback. The mesh uses only classes 0..2; the cache reads all eight.
/// Borrowed records and sound tables live with this room overlay.
extern WorldCollisionSurfaceProperties* gDryfieldBackStreetSurfaceProperties[8];

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
