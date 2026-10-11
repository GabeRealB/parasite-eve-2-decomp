#ifndef INCLUDE_ROOMS_SHELTER_1F_PARKING_GARAGE_H
#define INCLUDE_ROOMS_SHELTER_1F_PARKING_GARAGE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Number of retained placement-variant slots, including absent slots.
enum { SHELTER_1F_PARKING_GARAGE_AREA_VARIANT_COUNT = 12 };

/// Shelter 1F parking garage placement/resource directory indexed by `GameLocationKey.variant`.
///
/// Variant 1 publishes empty placement and resource lists.
/// All other slots, including 0, are absent (NULL, NULL).
/// Placements belong to the Neo Ark map overlay; recipes belong to this room.
/// Keep both and any actors supplying task descriptors loaded through loading,
/// spawning and model-flag updates.
/// Selectors must be in 0..11; there is no terminator or bounds check.
extern AreaVariant gShelter1fParkingGarageAreaVariants[SHELTER_1F_PARKING_GARAGE_AREA_VARIANT_COUNT];

/// Logical views and identity-mapped camera/image/sprite slots in the garage.
enum { SHELTER_1F_PARKING_GARAGE_VIEW_COUNT = 4 };

/// Room-1 collision mesh, view boundaries and interactions, with no sight occluders.
///
/// Gameplay borrows and mutates the grid and trigger lists while this overlay
/// is loaded; unlink the lists before unloading the room.
extern WorldCollisionRoomResources gShelter1fParkingGarageCollisionRooms[1];

/// Room-1 lighting: four point lights and minimum ambient colours for four views.
///
/// Light coordinates and attenuation remain writable while the room is loaded.
extern WorldCoordRoomLighting gShelter1fParkingGarageRoomLighting[1];

/// Room-1 logical views 1..4 mapped to identical camera, image and sprite slots.
///
/// The room-owned directory borrows gameplay's `gViewIdentityMap`.
extern u8* gShelter1fParkingGarageViewMaps[1];

/// Room-1 logical-view count, limiting reverse searches through its view map.
extern ViewCount gShelter1fParkingGarageViewCounts[1];

/// Two 1-based arrival/departure endpoints: underground parking and the airlock.
///
/// Supplies player/companion placements, initial views 2 and 4, sound requests
/// and the underground-parking map marker. Borrowed while this room is loaded.
extern DirectionWarpEntry gShelter1fParkingGarageWarps[2];

/// Four mapped cameras with Q12 world rotations, world origins and pixel perspective.
///
/// Indexed by mapped view minus one; queued application requires the overlay
/// to remain loaded until the camera is copied.
extern ViewCamera gShelter1fParkingGarageViewCameras[SHELTER_1F_PARKING_GARAGE_VIEW_COUNT];

/// Four mapped sprite views: empty views 1/4 and foreground sprites in views 2/3.
///
/// Indexed by mapped view minus one. All select decoded background strips;
/// packet sources and batch lists are borrowed until drawing is finished.
extern SpriteView gShelter1fParkingGarageSpriteViews[SHELTER_1F_PARKING_GARAGE_VIEW_COUNT];

/// Eight surface-class slots: class 1 has footsteps; all block probes and allow impacts.
///
/// Every class permits collision pushback. The records and sound requests are
/// borrowed from this overlay and remain valid only while it is loaded.
extern WorldCollisionSurfaceProperties* gShelter1fParkingGarageSurfaceProperties[8];

/// Runs the parking garage's room controller for one tick.
///
/// Requires a live bodyless task with state 0..2 and this room loaded. State 0
/// installs room messages and handles warp-1 arrival, state 1 remains idle for
/// messages, and state 2 requests teardown. Allocates no work of its own.
void shelter1fParkingGarageRoomTask(Task* task);

/// Installs the room's actor-effect IDs and draws fixed glows in mapped views 2 and 4.
///
/// Requires a live room-effect controller, composed view matrices and frame
/// packet/scratch capacity. State 0 publishes the flash, twin-trail and spark-burst
/// IDs, then becomes state 1. View 2 draws four amber capsules; view 4 draws a red
/// disc and two amber capsules. Other views draw none. Radius scales are 512 for
/// capsules and 768 for the disc, projected as scale * 64 / (camera Z / 4) pixels.
/// Each capsule's first depth is clamped to 16; its second end and the disc require
/// depth at least 17. Room effect control does not gate this task.
void shelter1fParkingGarageDrawViewGlowsTask(Task* task);

/// Runs the room's charging pink flash, peak screen tint and fading star.
///
/// Requires the coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn`. `spawnArg1.value` is a positive charge duration in callback
/// ticks, consumed as a countdown after initialization. Nonzero room effect
/// control pauses the task; values 4 and above cancel it. State 3 requests
/// release. Completion or cancellation releases the counted work and task.
void shelter1fParkingGarageRoomVisualEffectsFlashTask(Task* task);

/// Records two moving endpoints and draws their fading blue twin-trail beam.
///
/// Requires the coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn`, a live parent coordinate, and `Task::work` initially NULL.
/// Borrows the parent for its lifetime and owns two eight-coordinate histories
/// in `Task::work`; allocation failure retries initialization. `spawnArg1.value`
/// is zero for an unlimited lifetime, or 2..32767 for a release age in active
/// ticks. Initialization uses age 1 without checking expiry. Room effect control
/// values 2 and above freeze updates and drawing. Effect teardown releases both
/// the counted work and history block.
void shelter1fParkingGarageRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs the parking garage's impact flash and smoke or orange-ring spark burst.
///
/// Requires a counted coordinate-body effect with zero-aged owned `EffectWork`
/// in `spawnArg2.pointer` and state 0, as supplied by `effectSpawn`.
/// Nonzero `spawnArg1.value` emits smoke through active age 7; zero starts two
/// bouncing sparks and fading rings. Age 7 enters release; the next active tick
/// frees the work, body and task. Nonzero room effect control pauses below 4
/// and cancels at 4 or above. Keep this overlay and frame resources loaded;
/// spawned child effects have independent lifetimes.
void shelter1fParkingGarageRoomVisualEffectsSparkBurstTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_1F_PARKING_GARAGE_H
