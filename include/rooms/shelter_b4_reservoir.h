#ifndef INCLUDE_ROOMS_SHELTER_B4_RESERVOIR_H
#define INCLUDE_ROOMS_SHELTER_B4_RESERVOIR_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Four patrol-ring selections borrowed by the reservoir's Bog Divers.
///
/// Spawn bits 4..5 select entry 0..3; all four point to the same eight-point
/// ring. XYZ use game-coordinate units; swimming adds the sampled water Y to
/// each waypoint's depth. The room and ring must outlive every borrowing diver.
extern SVECTOR* gShelterB4ReservoirDiverPatrolRings[4];

/// Writable emergence spots shared by the reservoir's Bog Divers.
///
/// Entry 0 is the fallback claim; entries 1..3 are searched by XZ distance.
/// `pad` is 0 free, 1 claimed or -1 end (entry 4). Claims are released when
/// moved or on actor teardown. X/Z use world-coordinate units; the searches
/// do not read Y. The list must stay live and writable until every borrowing
/// diver has released its claim.
extern SVECTOR gShelterB4ReservoirDiverSurfaceSpots[5];

/// Main reservoir water-plane Y in world units, shared with the Bog Diver.
///
/// Room initialization selects -2000 before the event or -500 afterward;
/// the event ramp moves between them. The water task publishes this height to
/// the live session. Diver spawning snapshots its signed halfword value.
/// Storage is valid while the reservoir overlay is loaded.
extern s16 gShelterB4ReservoirMainWaterY;

/// Reservoir area layouts indexed directly by placement variant 0..11.
///
/// Variants 1, 2, 3 and 11 contain paired placement/resource lists; other
/// slots are absent. Each list ends with `AREA_PLACEMENT_END`. Gameplay's
/// stage-4 area-45 record borrows these tables while the room is loaded;
/// actors may retain their placement records until teardown.
extern AreaVariant gShelterB4ReservoirAreaVariants[12];

/// Surface classes at each completed step of the reservoir's ten-step ascent.
///
/// Gameplay's stair flight 4 reads entries 0..10, including the final landing.
/// Entries 11..15 are retained with no established use. Borrowed while this
/// room is loaded; this row has no terminator.
extern u8 gShelterB4ReservoirStairAscentSurfaces[16];

/// Surface classes at each completed step of the reservoir's ten-step descent.
///
/// Gameplay's stair flight 4 reads entries 0..10, including the final landing.
/// Entries 11..15 are retained with no established use. Borrowed while this
/// room is loaded; this row has no terminator.
extern u8 gShelterB4ReservoirStairDescentSurfaces[16];

/// Lighting selections for reservoir room IDs 1 and 2, indexed by room minus one.
///
/// Both borrow the same eight-point-light collection and use the resident
/// default ambient minima. Records and lights stay live while the room is loaded.
extern WorldCoordRoomLighting gShelterB4ReservoirRoomLighting[2];

/// Collision selections for reservoir room IDs 1 and 2, indexed by room minus one.
///
/// Both share one grid, eight view boundaries and two sight occluders; each
/// selects its own action-trigger list. Setup mutates and links those resources.
/// The directory and all borrowed storage must remain live while active, and
/// the linked triggers/occluders must be unlinked before the room is unloaded.
extern WorldCollisionRoomResources gShelterB4ReservoirRoomCollisionResources[2];

/// Logical-view maps for reservoir room IDs 1 and 2, indexed by room minus one.
///
/// Both borrow `gViewIdentityMap`: logical views 1..10 select camera, image
/// and sprite entries 1..10. The maps have no terminator; the room directory
/// requires the reservoir overlay to stay loaded and the maps require gameplay.
extern u8* gShelterB4ReservoirViewMaps[2];

/// Logical-view counts for reservoir room IDs 1 and 2, indexed by room minus one.
///
/// Both search ten entries of the identity view map. There is no terminator;
/// the stage directory borrows the counts while the reservoir overlay is loaded.
extern ViewCount gShelterB4ReservoirRoomViewCounts[2];

/// Reservoir warp endpoints 1 and 2, indexed by warp ID minus one.
///
/// Endpoint 1 places arrivals in view 2 by the water-supply passage and plays
/// room-bank sounds 4 on arrival and 3 on departure. Endpoint 2 places arrivals
/// in view 7 by the upper-sewer passage, fades on departure and marks the map.
/// World positions and 4096-unit yaw are copied for player/companion placement.
/// There is no terminator; the stage directory borrows the loaded room's entries.
extern DirectionWarpEntry gShelterB4ReservoirWarpEntries[2];

/// Cameras for mapped reservoir views 1..10, indexed by mapped view minus one.
///
/// Each record supplies world-to-camera placement and perspective distance in
/// pixels. Queued camera application borrows its record until dispatch, so the
/// reservoir overlay must stay loaded through application.
extern ViewCamera gShelterB4ReservoirViewCameras[10];

/// Foreground sprite resources for mapped reservoir views 1..10.
///
/// Indexed by mapped view minus one; views 1, 2, 6 and 10 have no sprites.
/// Every view retains decoded-image background strips and has no clipping list.
/// Batch lists end with `SPRITE_BATCH_END`; source ranges count elements.
/// Descriptors and their mutable arrays belong to the loaded reservoir overlay.
extern SpriteView gShelterB4ReservoirSpriteViews[10];

/// Collision policies and footstep requests for reservoir surface classes 0..7.
///
/// Class 1 passes probes and ignores weapon impacts; the others block probes
/// and allow impacts. Every class applies pushback. Classes 2, 3 and 5 have
/// footstep cues; 0, 4, 6 and 7 share the silent default record. The directory
/// and borrowed records remain live only while the reservoir overlay is loaded.
extern WorldCollisionSurfaceProperties* gShelterB4ReservoirSurfacePropertiesByClass[8];

/// Runs the reservoir room's initialization, update or teardown state.
///
/// Requires state 0..2: initialization binds messages, water rendering and
/// the event model; state 1 refreshes retained spray arguments; state 2 kills
/// the task. Copies its handler table before unchecked dispatch. The reservoir
/// overlay and its room resources must remain loaded while this callback runs.
void shelterB4ReservoirRoomTask(Task* task);

/// Runs one six-cell burst sprite emitted by the reservoir event.
///
/// Requires a counted effect task with a live coordinate body, owned zeroed
/// `EffectWork` in `spawnArg2.pointer`, and initial state and frame index zero.
/// `spawnArg1` bits 0..11 give the world-unit perspective half-extent; bits
/// 12..14 give running updates per cell (an entirely zero bits 12..15 field
/// selects 1). A nonzero period field must have bits 12..14 nonzero: bit 15
/// alone decodes to zero and would reach remainder by zero. Bits 16..23 give
/// speed in parent-coordinate units per update (0 selects 64); higher bits
/// are ignored. Initialization consumes one random draw and seeds motion
/// along negative parent X without drawing. Later updates draw, move, and
/// advance through cells 0..5; the coordinate-list pass refreshes the world
/// matrix between updates. Non-running control redraws without aging;
/// cancellation redraws once before releasing the work and task.
void shelterB4ReservoirBurstSpriteTask(Task* task);

/// Runs the reservoir's player splashes, event bursts, view spray and glow drawing.
///
/// Requires a counted effect task with zeroed owned `EffectWork` in `spawnArg2`,
/// initial state 0, the live player TMD with at least 18 coordinates and live
/// room-effect state. Initializes the room's effect IDs, ten burst positions
/// and two cached player positions. Coordinates 14 and 17 supply splash samples;
/// their anatomical identity is unproven. Cached view-space positions narrow
/// to halfwords; initialization reads the existing matrices without composing.
/// After the reservoir event, running updates age the splash gate and compare
/// motion against rolls out of 512. Configured bursts roll out of 100 per point.
/// Mapped view 10 emits spray/ripples even during pause or cancellation, and
/// per-view glows always draw. This task has no cancellation or retirement arm;
/// the effect exit callback owns teardown. Burst offsets live for the room,
/// and `effectSpawn` snapshots temporary splash placement during each call.
void shelterB4ReservoirAmbientEffectsTask(Task* task);

/// Runs the reservoir's expanding, fading water-surface ripple.
///
/// Requires a counted effect task with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body and initial state zero. `spawnArg1` bits 0..11 give the
/// initial local half-side in coordinate units; higher bits are ignored.
/// Running updates grow by 32 and dim brightness by 2, lasting 32 updates.
/// Non-running control redraws retained state; cancellation redraws once
/// before releasing the work and task. The coordinate's parent must stay live.
void shelterB4ReservoirWaterRippleTask(Task* task);

/// Runs the reservoir's eight-cell water-spray particle with gravity.
///
/// Requires a counted effect task with owned zeroed `EffectWork` in
/// `spawnArg2.pointer`, a coordinate body, initial state and frame index zero.
/// `spawnArg1` bits 0..11 give the perspective size scale, bits 12..15 updates
/// per cell (0 selects 1), and bits 16..23 parent-space speed (0 selects 64).
/// Bits 24..27 select velocity: 0 stationary, 1 upward burst, 2 all-axis spray,
/// 3 narrow upward jet, 5 direction from the copied spawn offset in `pos`;
/// other kinds retain a zero direction before normalization. Independently,
/// any set bits 28..31 select upright drawing; otherwise drawing is rotated.
/// A preseeded nonzero `move` bypasses speed and direction generation.
/// Initialization composes without drawing; subsequent running updates draw,
/// move and add 6 to signed-halfword Y velocity. Non-running control freezes
/// motion and redraws; cancellation releases the work and task without drawing.
void shelterB4ReservoirWaterDriftTask(Task* task);

/// Runs the reservoir's attached charge disc, player-joint sparks and release ring.
///
/// Requires a counted effect task with a coordinate body, owned zeroed
/// `EffectWork` in `spawnArg2.pointer` and initial state
/// `ROOM_VISUAL_EFFECTS_GLOW_DISC_ATTACH`. `spawnArg1.value` selects tint 0 or 1.
/// The work's borrowed parent coordinate and ancestors must remain live; `pos`
/// is the copied offset in that parent's coordinate units. Growing emits adopted
/// flying sparks from player model parts 3..18 every fourth active age, requiring
/// the live player model and installed flying-spark effect ID. The owner requests
/// flicker, release or cancel through `ROOM_VISUAL_EFFECTS_GLOW_DISC_*` states.
/// Nonzero room-effect control pauses; four or above cancels. Release completion
/// or cancellation frees the work, task and adopted sparks. The reservoir overlay
/// must stay loaded while the task runs.
void shelterB4ReservoirRoomVisualEffectsGlowDiscTask(Task* task);

/// Runs the reservoir's glowing-disc spark toward an initial target coordinate.
///
/// Requires a counted effect task with owned `EffectWork` in `spawnArg2.pointer`
/// and a coordinate body. `spawnArg1.pointer` borrows a `GfxCoord` through the
/// first running update; both world matrices must be composed then. That update
/// fixes a step at 204/4096 of the initial target displacement in parent axes.
/// Later updates move by that step and draw on odd ages, releasing the work
/// and task at age 20. Non-running control pauses without drawing; cancellation
/// releases the effect. The target is not sampled after initialization.
void shelterB4ReservoirRoomVisualEffectsFlyingSparkTask(Task* task);

/// Runs the reservoir's orange burst with a growing disc, glow and fading ring.
///
/// Requires a counted effect task with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body and initial state zero; `spawnArg1` is ignored. Running
/// updates compose, expand the glow and fade the ring before the central disc.
/// Non-running control pauses without drawing. Cancellation or completed fading
/// releases the work and task; callers must not retain those released pointers.
void shelterB4ReservoirRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B4_RESERVOIR_H
