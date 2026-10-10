#ifndef INCLUDE_ROOMS_SHELTER_B4_LOWER_SEWER_H
#define INCLUDE_ROOMS_SHELTER_B4_LOWER_SEWER_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Four borrowed Bog Diver patrol rings, selected by spawn-argument bits 4..5.
///
/// Each ring has eight points and wraps at entry 7. Slots 0/2 share the Z=1000
/// ring; 1/3 share Z=2000. XYZ use room units; swimming goal Y adds the sampled
/// water height to unsigned waypoint Y. Keep this room loaded while divers live.
extern SVECTOR* gShelterB4LowerSewerDiverPatrolRings[4];

/// Writable Bog Diver emergence spots, with entry 0 reserved as the claim fallback.
///
/// Entries 1..8 supply room-space XZ; their Y is not read by the spot searches.
/// `SVECTOR.pad` is a mutable claim mark (0 free, 1 claimed, -1 terminator).
/// Entry 9 terminates the search and is never a spot. Divers release claims on
/// moving or despawning; keep this room loaded through every diver's lifetime.
extern SVECTOR gShelterB4LowerSewerDiverSurfaceSpots[10];

extern s16 D_shelter_b4_lower_sewer_80181E6C;

/// Enemy layouts indexed by the saved lower-sewer placement variant, 0..11.
///
/// Slots 1, 2, 3, 7, 8 and 11 pair terminated placement and resource lists;
/// slots 0, 4, 5, 6, 9 and 10 are empty. Variant 1 is the default. Loading and
/// spawned actors borrow the lists, so this room must stay loaded while used.
extern AreaVariant gShelterB4LowerSewerAreaVariants[12];

// shelter_b4_lower_sewer
/// Logical-view maps for the lower sewer's single room, indexed by room ID minus one.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1..9 select matching camera,
/// image and sprite indices. The directory requires this room overlay to remain loaded.
extern u8* gShelterB4LowerSewerViewMaps[1];

/// Reverse-lookup view counts for the lower sewer's single room, indexed by room minus one.
///
/// Room 1 searches nine logical views in its identity map. The signed-halfword
/// element counts have no terminator and are borrowed while this room is loaded.
extern ViewCount gShelterB4LowerSewerRoomViewCounts[1];

/// Four lower-sewer arrival/departure endpoints, indexed by warp ID minus one.
///
/// XYZ use world units and yaw uses 4096 units per turn. Arrival views are
/// 5, 2, 9 and 6. Endpoint 1 marks the reservoir map flag; endpoints 2 and 4
/// supply arrival/departure sounds. No terminator; borrow while the room is loaded.
extern DirectionWarpEntry gShelterB4LowerSewerWarpEntries[4];

/// Lower-sewer room-space collision mesh and its eight-by-two XZ cell index.
///
/// Cells span 4000 game units with origin (-14760, -3210). The mesh owns 100
/// vertices, 16 Q12 normals and 33 faces using surface classes 0, 1, 2, 3 and 5.
/// Activation binds the view transform; the descriptor and pools stay writable
/// and valid only while this room overlay is loaded.
extern WorldCollisionGrid gShelterB4LowerSewerCollisionGrid;

/// Cameras for the lower sewer's nine mapped views, indexed by mapped view minus one.
///
/// Matrices use Q12 rotation and negated world-space camera origins; perspective
/// distance is 207 pixels for view 1 and 257 for views 2..9. There is no terminator.
/// Camera application borrows each record while this room overlay is loaded.
extern ViewCamera gShelterB4LowerSewerViewCameras[9];

/// Sprite descriptors for the lower sewer's nine mapped views, indexed by mapped view minus one.
///
/// Views 4, 5, 6 and 9 add opaque depth-sorted rectangles to the decoded background;
/// the other views retain empty sprite lists. All use unclipped drawing. These
/// mutable descriptors and their lists require this room overlay to remain loaded.
extern SpriteView gShelterB4LowerSewerSpriteViews[9];

/// The lower sewer's fourteen authored point lights for actor/model shading.
///
/// Lights contribute in every view with world-unit falloff radii. Setup binds
/// them to the current view; composition and queries update their caches.
/// Keep this room overlay loaded while the collection is bound.
extern WorldCoordRoomLights gShelterB4LowerSewerRoomLights;

/// Twelve directed camera boundaries between lower-sewer views 2..9.
///
/// The source/destination view bytes select a change only from the source view.
/// The final boundary carries LAST and is included. Setup binds and links these
/// writable room-space quads; keep their storage loaded until the list is cleared.
extern WorldCollisionTrigger gShelterB4LowerSewerViewBoundaryTriggers[12];

/// Twelve lower-sewer stair, warp and CAP interaction quads.
///
/// Two facing actions encode 5 steps with surface classes 3 and 5 at yaw 3072.
/// Four warps lead to the incinerator control room, elevator hall or upper sewer;
/// six CAP regions select commands 1..4. The final included quad carries LAST.
/// Setup binds and links these writable room-space records; keep this room
/// loaded until their action list is cleared.
extern WorldCollisionTrigger gShelterB4LowerSewerActionTriggers[12];

/// Lower-sewer collision and footstep policies indexed by surface class 0..7.
///
/// Class 1 passes probes and suppresses weapon impacts while retaining pushback;
/// every other class blocks probes and permits impacts. Classes 2, 3 and 5
/// have paired footstep cues; 0, 1, 4, 6 and 7 are silent. Every class applies
/// pushback. The directory and records require this room overlay to remain loaded.
extern WorldCollisionSurfaceProperties* gShelterB4LowerSewerSurfacePropertiesByClass[8];

/// Runs the lower sewer's room setup, idle or teardown state.
///
/// Requires a live task with state 0..2 and this room and the Shelter map overlay
/// loaded. State 0 installs the room messages and room task slot, starts water
/// when the reservoir event is complete, then enters the persistent idle state.
/// State 2 releases the task. The body and spawn arguments are unused.
void shelterB4LowerSewerRoomTask(Task* task);

/// Runs the lower sewer's charging pink flash, peak screen tint and fading star.
///
/// Requires a counted effect with a coordinate body, initial state 0 and owned
/// `EffectWork` in `spawnArg2.pointer`. `spawnArg1.value` is a positive charge
/// duration in running updates, consumed as a countdown. Nonzero room effect
/// controls pause without drawing; controls 4 and above, state 3 or fade
/// completion release the work, task and body and decrement the effect count.
/// Coordinate ancestors and this overlay must remain live, with initialized
/// scratch storage and frame packet space available for drawing.
void shelterB4LowerSewerRoomVisualEffectsFlashTask(Task* task);

/// Records two moving endpoints and draws the lower sewer's fading blue twin trails.
///
/// Requires a coordinate body, zeroed counted `EffectWork` in `spawnArg2.pointer`,
/// initial state 0, `Task::work` initially NULL and a live `EffectWork::parent`.
/// Owns two eight-coordinate world-space histories in `Task::work`; allocation
/// failure retries initialization. The endpoints use this room's fixed local
/// offsets from the parent. `spawnArg1.value` 0 leaves the lifetime unlimited;
/// 2..32767 retire at that signed-halfword age in updates with control below 2.
/// Control values 2 and above skip updates and drawing, including retirement.
/// Teardown frees both allocations and the body and decrements the effect count.
/// The parent, view coordinate and overlay must remain live, with initialized
/// scratch storage and frame packet space available for drawing.
void shelterB4LowerSewerRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs the lower sewer's impact flash with smoke or orange rings and bouncing sparks.
///
/// Requires a coordinate body, zero-aged counted `EffectWork` in
/// `spawnArg2.pointer`, initial state 0 and live coordinate ancestors.
/// Nonzero `spawnArg1.value` selects smoke; zero selects rings and two sparks.
/// Age seven enters release, and age eight frees work and the body. Spawned
/// effects are independent. Nonzero room control below four pauses it; four
/// or above cancels it. Keep the overlay and effect resources loaded.
void shelterB4LowerSewerRoomVisualEffectsSparkBurstTask(Task* task);

/// Advances and draws an expanding, fading water-surface ripple for the lower sewer.
///
/// Requires a counted effect with a coordinate body, zeroed owned `EffectWork`
/// in `spawnArg2.pointer` and initial state 0. `spawnArg1` bits 0..11 give the
/// initial local half-side in coordinate units (0..4095); higher bits are ignored.
/// Each running update grows it by 32, draws, then dims by 2, for 32 draws at
/// brightness 64..2. The first update sets a random yaw, composed on the next.
/// Non-running controls redraw retained values; cancellation draws once before
/// retirement. Retirement frees the work, task and body and decrements the effect
/// count. Coordinate ancestors, this overlay and its water textures must remain
/// live, with initialized scratch storage and frame packet space for drawing.
void shelterB4LowerSewerWaterRippleTask(Task* task);

/// Advances and draws an eight-cell water-spray particle for the lower sewer.
///
/// Requires a counted effect with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body, initial state 0 and animation index 0. `spawnArg1` bits
/// 0..11 give perspective size (0..4095), 12..15 updates per cell (0 selects 1),
/// and 16..23 launch speed in parent-coordinate units per update (0 selects 64).
/// Bits 24..27 choose direction (0 stationary, 1 upward burst, 2 all-axis spray,
/// 3 narrow upward jet, 5 spawn-offset direction; others leave it zero).
/// A supplied nonzero `EffectWork::move` bypasses generation and scaling.
/// Nonzero bits 28..31 select an upright tile; otherwise a random angle is
/// retained at 4096 units per turn. The drawers receive unsigned cell indices.
///
/// The first running update initializes without drawing or moving. Later updates
/// draw, move and add 6 to signed-halfword Y velocity, retaining its low 16 bits;
/// stationary particles skip movement and gravity. Cells 0..7 each last the
/// decoded period. Suspended controls redraw; cancellation retires without drawing.
/// Retirement frees the work, task and body and decrements the effect count.
/// Coordinate ancestors, this overlay and its water textures must remain live,
/// with initialized scratch storage and frame packet space for drawing.
void shelterB4LowerSewerWaterDriftTaskU16(Task* task);

/// Installs the lower sewer's actor-effect IDs and draws its visible capsule glows.
///
/// Initial state 0 installs the flash, twin-trail and spark-burst IDs once, plus
/// the water IDs when the reservoir-completion flag is exactly 1. Every tick
/// draws additive grey glows for mapped views 2..9; view 1 draws none. Requires
/// this loaded room, a composed view matrix, initialized scratch storage and
/// current frame packet space and ordering table. Body and spawn arguments are
/// unused; the task stays live independently of room effect control.
void shelterB4LowerSewerDrawGlowsTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B4_LOWER_SEWER_H
