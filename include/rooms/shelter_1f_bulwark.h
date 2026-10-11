#ifndef INCLUDE_ROOMS_SHELTER_1F_BULWARK_H
#define INCLUDE_ROOMS_SHELTER_1F_BULWARK_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Number of retained placement-variant slots, including absent slots.
enum { SHELTER_1F_BULWARK_AREA_VARIANT_COUNT = 12 };

/// Shelter 1F bulwark placement/resource directory indexed by `GameLocationKey.variant`.
///
/// Variant 1 places a Beam Sword Rook and a grenade-launcher Rook.
/// All other slots, including 0, are absent (NULL, NULL).
/// Placements belong to the Neo Ark map overlay; recipes belong to this room.
/// Keep both and any actors supplying task descriptors loaded through loading,
/// spawning and model-flag updates.
/// Selectors must be in 0..11; there is no terminator or bounds check.
extern AreaVariant gShelter1fBulwarkAreaVariants[SHELTER_1F_BULWARK_AREA_VARIANT_COUNT];

/// Collision resources for room 1: the grid, view boundaries, actions and sight quads.
///
/// Indexed by room - 1 through the Neo Ark map directory. Setup binds and
/// links the mutable resources; keep this overlay loaded until they are unlinked.
extern WorldCollisionRoomResources gShelter1fBulwarkCollisionResources[1];

/// Room 1's seven point lights, with the default ambient minima.
///
/// Indexed by room - 1. Light transforms and attenuation remain writable;
/// the collection and all referenced light storage belong to this overlay.
extern WorldCoordRoomLighting gShelter1fBulwarkRoomLighting[1];

/// Room 1's logical-view map, borrowing gameplay's shared identity map.
///
/// Indexed by room - 1; logical views 1..3 select matching camera, image and
/// sprite indices. Keep both gameplay and this room overlay loaded.
extern u8* gShelter1fBulwarkRoomViewMaps[1];

/// Reverse-lookup limit of three logical views for room 1, indexed by room - 1.
extern ViewCount gShelter1fBulwarkRoomViewCounts[1];

/// Bulwark arrival placements and departure cues for the airlock and heliport exits.
///
/// Indexed by warp - 1: endpoint 1 starts in view 2 at the vehicular airlock,
/// endpoint 2 in view 3 at the heliport. The first updates the airlock map marker.
/// The stage directory borrows these entries while this overlay is loaded.
/// Positions are world units and yaw uses 4096 per turn.
extern DirectionWarpEntry gShelter1fBulwarkWarpEntries[2];

/// Cameras for mapped views 1..3, indexed by mapped view - 1.
///
/// Rotation coefficients use 4096 for 1.0, translation uses world units and
/// projection distances are pixels. Keep this overlay loaded until application.
extern ViewCamera gShelter1fBulwarkCameras[3];

/// Sprite resources for mapped views 1..3, indexed by mapped view - 1.
///
/// Views 1 and 2 have no sprites; view 3 has eight depth-sorted rectangles.
/// All three retain decoded background strips and have no clipping-area list.
/// Sources and mutable batches belong to this overlay and stay live while drawing.
extern SpriteView gShelter1fBulwarkSpriteViews[3];

/// Collision policies indexed by room-local surface class, 0..7.
///
/// All classes block probes, allow weapon impacts and apply pushback. Class 1
/// is the grid's floor and supplies footstep cues; other classes are silent.
/// All pointers borrow storage from this room overlay.
extern WorldCollisionSurfaceProperties* gShelter1fBulwarkSurfaceProperties[8];

/// Runs the Shelter 1F Bulwark room controller for one tick.
///
/// `task` must be live with state 0..2 and this room overlay loaded.
/// Initialize room messages, remain idle between messages, then kill.
/// State 0 registers the borrowed task in `GAME_TASK_SLOT_ROOM`; state 2
/// requests teardown. The controller allocates no work or body of its own.
void shelter1fBulwarkRoomTask(Task* task);

/// Runs the Bulwark's charging pink flash, peak screen tint and fading star.
///
/// Requires a counted effect with a coordinate body, owned zeroed `EffectWork`
/// in `spawnArg2.pointer` and initial state 0, as supplied by `effectSpawn`.
/// `spawnArg1.value` is a positive charge duration in running updates, consumed
/// as a countdown after initialization. Controls 1..3 pause without drawing;
/// control 4 and above cancels. A running update in state 3 or fade completion
/// also releases the work, task and body and decrements the effect count. Keep
/// the room overlay loaded, with current view transforms, scratch space and
/// frame packet storage.
void shelter1fBulwarkRoomVisualEffectsFlashTask(Task* task);

/// Records and draws the Bulwark's fading twin trails from two anchor-relative endpoints.
///
/// Requires a counted effect with a coordinate body, owned `EffectWork` in
/// `spawnArg2.pointer`, age 0, initial state 0 and null `Task::work`, as supplied
/// by `effectSpawn`.
/// The work's borrowed parent coordinate must remain live. Initialization
/// allocates two eight-coordinate histories in `Task::work`; allocation failure
/// resets age for a later retry. Each later update records both endpoints and
/// draws seven fading quads from their world-space snapshots.
///
/// `spawnArg1.value` is 0 for external teardown, or 2..32767 for release at that
/// active-update age; initialization occupies age 1. Age is a signed halfword
/// and wraps. Controls below 2 advance and draw; other controls retain the task
/// without drawing or cancellation. Teardown frees both histories, the effect
/// work and the coordinate body and decrements the effect count. Keep the room
/// overlay loaded, with current view transforms, scratch and frame packet space.
void shelter1fBulwarkRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs the Bulwark's impact flash and smoke or orange-ring spark burst.
///
/// Requires a counted coordinate-body effect with zero-aged owned `EffectWork`
/// in `spawnArg2.pointer` and state 0, as supplied by `effectSpawn`.
/// Nonzero `spawnArg1.value` emits smoke through active age 7; zero starts two
/// bouncing sparks and fading rings. Age 7 enters release; the next active tick
/// frees the work, body and task. Nonzero room effect control pauses below 4
/// and cancels at 4 or above. Keep this overlay and frame resources loaded;
/// spawned child effects have independent lifetimes.
void shelter1fBulwarkRoomVisualEffectsSparkBurstTask(Task* task);

/// Draws the Bulwark's fixed glows and installs its room-specific combat effects.
///
/// State 0 selects the flash, twin-trail and spark-burst effect IDs, then enters
/// state 1. Drawing starts on that same update: mapped view 2 has four glows,
/// view 3 has one, and other views draw none. The radius inputs are world units;
/// red, green and blue intensity factors multiply the frame-parity pulse.
/// Spawn arguments and the task's coordinate body are unused. Keep the room
/// overlay loaded, with a current view matrix, scratch and frame packet space.
void shelter1fBulwarkDrawGlowsTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_1F_BULWARK_H
