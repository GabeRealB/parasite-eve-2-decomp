#ifndef INCLUDE_ROOMS_SHELTER_B3_DUMPING_HOLE_H
#define INCLUDE_ROOMS_SHELTER_B3_DUMPING_HOLE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"
#include "overlay.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// Twelve enemy-entry spots around the dumping hole's two opposing walls.
///
/// Commands select indices 0..11 in their high byte. XYZ are signed world units;
/// heading uses 4096 units per turn. All spots lie at Y -3950; rows 0..5 and
/// 6..11 face opposite ways. Actor overlays borrow the table while this room is loaded.
extern OverlayEncounterSpot gShelterB3DumpingHoleEncounterSpots[12];

extern TmdSource gShelterB3DumpingHoleItemBoxModel;

enum { SHELTER_B3_DUMPING_HOLE_ENCOUNTER_TASK_CONTROLLER = 0 };

/// Bodyless tasks for the encounter controller, Mad Chaser, Slouch and Sucklerceph pair.
///
/// Entries 0..3 have priorities 32, 97, 97 and 97. Controller argument 1 is
/// unused. Spawner argument 1 packs slot 0..15 in its high halfword and the
/// appearance command in its low halfword; argument 2 is unused. Callbacks
/// allocate task-owned work and borrow room tables, which must remain loaded.
extern TaskDesc gShelterB3DumpingHoleEncounterTaskDescriptors[4];

/// One bodyless task recipe for the dumping-hole encounter's scene-clock captions.
///
/// Spawn index 0 runs the room's caption schedule at priority 32. Argument 1's
/// low signed halfword is the bottom baseline in screen pixels (normally 208);
/// argument 2 is unused. The task owns no work and does not end itself. Keep the
/// room's relocated CAP command/key data, textures and callbacks loaded until teardown.
extern TaskDesc gShelterB3DumpingHoleEncounterCaptionTaskDescriptors[1];

/// Placement/resource variants for Mine/Shelter area 38, the dumping hole.
///
/// Variant 1 supplies the debris scene, 2 the encounter, and 10 the Glutton
/// alone; every other slot is absent. Indexed by placement variant.
/// Gameplay borrows the directory and its tables while this overlay is loaded.
extern AreaVariant gShelterB3DumpingHoleAreaVariants[13];

/// Collision-resource directory for dumping-hole rooms 1 and 2, indexed by room minus one.
///
/// Both use the same grid and have distinct mutable action-trigger arrays.
/// Room 2 also supplies view boundaries; neither room supplies sight occluders.
/// The Shelter stage map borrows this directory while the room overlay is loaded.
extern WorldCollisionRoomResources gShelterB3DumpingHoleRoomCollisionResources[2];

/// Logical-view map directory for dumping-hole rooms 1 and 2.
///
/// Both borrow the resident identity map for logical views 1..37. The directory
/// stays valid only while this room overlay is loaded.
extern u8* gShelterB3DumpingHoleViewMaps[2];

/// Logical-view counts for dumping-hole rooms 1 and 2.
///
/// Indexed by room minus one; both identity maps cover 37 logical views.
/// The Shelter stage directory borrows this array while the room is loaded.
extern ViewCount gShelterB3DumpingHoleViewCounts[2];

/// Three dumping-hole warp endpoints, indexed by one-based warp ID minus one.
///
/// Both actors arrive at the same world position and yaw. Warp 1 has arrival
/// and departure sounds; warps 2 and 3 are silent. Default views are 30, 30 and
/// 15. The Shelter stage map borrows these records while the room stays loaded.
extern DirectionWarpEntry gShelterB3DumpingHoleWarpEntries[3];

/// Cameras for the dumping hole's 37 mapped views, shared by both rooms.
///
/// Indexed by mapped view minus one. Each record supplies a world-to-camera
/// transform and pixel projection distance; queued use requires the overlay live.
extern ViewCamera gShelterB3DumpingHoleViewCameras[37];

/// Sprite descriptors for dumping-hole mapped views 1..37.
///
/// Indexed by mapped view minus one. Views 13 and 37 share sources and batches;
/// only view 13 supplies clipping commands. Lists and mutable sprites are owned
/// by the loaded room overlay and must stay live while drawing.
extern SpriteView gShelterB3DumpingHoleSpriteViews[37];

/// Dumping-hole room index 1's two directional and three point lights, with no cone lights.
///
/// The loaded room overlay owns the record and its mutable coordinate/query
/// storage. Selected by the map's room-lighting array; pointers must not outlive it.
extern WorldCoordRoomLights gShelterB3DumpingHoleRoom1Lights;

/// Dumping-hole room index 2's twelve point lights, with no cone lights.
///
/// The loaded room overlay owns the record and its mutable coordinate/query
/// storage. Selected by the map's room-lighting array; pointers must not outlive it.
extern WorldCoordRoomLights gShelterB3DumpingHoleRoom2Lights;

/// Minimum ambient colours for dumping-hole room index 1's 37 views.
///
/// Entry zero counts the views; entries 1..37 use RGB levels in sixteenths of
/// an 8-bit channel. Out-of-range views use the gameplay default. The loaded
/// room overlay owns this read-only input and must remain loaded while used.
extern WorldCoordRoomAmbientEntry gShelterB3DumpingHoleRoom1AmbientByView[38];

/// Minimum ambient colours for dumping-hole room index 2's 37 views.
///
/// Entry zero counts the views; entries 1..37 use RGB levels in sixteenths of
/// an 8-bit channel. Out-of-range views use the gameplay default. The loaded
/// room overlay owns this read-only input and must remain loaded while used.
extern WorldCoordRoomAmbientEntry gShelterB3DumpingHoleRoom2AmbientByView[38];

/// Collision policies for dumping-hole surface classes 0..7.
///
/// Class 3 passes probes and ignores weapon impacts; class 4 supplies footsteps.
/// All classes apply pushback. The ninth NULL pointer is retained storage,
/// outside the eight-class lookup domain. Policies live in the loaded room overlay.
extern WorldCollisionSurfaceProperties* gShelterB3DumpingHoleSurfacePropertiesByClass[9];

/// Runs the dumping hole's room receiver through setup, idle and teardown.
///
/// Requires the loaded room and state 0..2. Setup installs the room message
/// table and `GAME_TASK_SLOT_ROOM`, selects caption resources and starts the
/// arrival event or encounter appropriate to the live session. State 1 leaves
/// messages active without per-frame work; state 2 kills the task. No body or
/// work allocation is required.
void shelterB3DumpingHoleRoomTask(Task* task);

/// Spawns a dark rising sprite at a coordinate's world position plus `worldOffset`.
///
/// The offset is copied in world units during this call; the coordinate is
/// lent through the task's first update. The sprite then moves independently.
/// Requires the debris event director to remain live; its actor-sprite stop
/// flag cancels the task. Allocation failure removes the spawned task.
void shelterB3DumpingHoleSpawnActorSprite(GfxCoord* sourceCoord, const SVECTOR* worldOffset);

/// Draws the dumping hole's world-space discs and capsules for the mapped camera view.
///
/// Exported to gameplay's room-effect table. Requires composed view matrices
/// and the frame packet arena and scratch stack used by the glow drawers.
/// On its first tick clears the room's conditional-glow flag; view 14 draws
/// its two gated points only while that flag is nonzero. Retains no pointer.
void shelterB3DumpingHoleDrawViewGlowsTask(Task* task);

/// Animates a drifting sprite with an optional parent-oriented launch.
///
/// Gameplay effect slot 0x199 supplies counted, task-owned `EffectWork` in
/// `spawnArg2.pointer` and a single-coordinate body. Start at state 0, cell 0;
/// initialization consumes one running update without drawing or moving.
/// The coordinate's `workm` must be composed before drawing; GTE, scratch-stack
/// and packet-arena state must be ready.
///
/// `spawnArg1` bits 0..11 set perspective size; bits 12..15 select the frame
/// period (zero means 1, otherwise bits 12..14 must be 1..7). Bits 16..23
/// select launch speed in coordinate-parent units per update (zero means 64).
/// Bits 24..27 select direction: 0 stationary, 1 random upward, 2 all axes,
/// 3 narrow upward, 5 from `work->pos`, 6 planar, 7 parent-rotated forward direction.
/// Other kinds retain the zero direction for SDK normalization. Mode 7 borrows
/// `work->parent->coord`'s rotation, which must map into the effect's parent
/// space. A nonzero initial `move` bypasses generation and speed scaling.
///
/// Bit 31 selects the ten-cell alternate sheet; clear selects the twelve-cell
/// banked sheet. Bits 28..30 become palette bits in `pos.vx`, replacing offset X
/// before mode 5 copies its direction. Spin is randomized in 4096 units per turn.
/// Running updates draw, move, then accelerate Y: mode 7 adds age/10; other
/// modes subtract 1 (alternate) or 2 (banked). Zero `step` skips movement and
/// acceleration. Animation advances once per period and releases after the
/// final cell. Suspension redraws without advancing; cancellation redraws
/// once before releasing the counted work and task through `effectKillTask`.
void shelterB3DumpingHoleEffectSpriteDriftTaskAimed(Task* task);

/// Animates an eight-frame chip or billboard particle shed by the Glutton rain effect.
///
/// Spawn argument 2 lends the effect spawner's counted `EffectWork`.
/// Argument 1 packs size in bits 0-11, frame period in 12-15 (zero means one tick), speed in 16-23
/// (zero means 64), and velocity kind in 24-27: 0 still, 1 random upward,
/// 2 random in all axes, 3 narrow upward, 5 direction from the work position.
/// A nonzero top nibble selects billboard drawing; zero selects a spinning
/// chip. An existing nonzero velocity bypasses direction generation.
/// Movement adds six world units per tick to Y velocity, then animation
/// advances at the selected period and releases after frame 7. Suspension
/// redraws without advancing; cancellation releases through `effectKillTask`.
void shelterB3DumpingHoleGluttonRainParticleTask(Task* task);

/// Attaches and animates the Glutton's falling rain blob, then emits its landing burst.
///
/// Requires a coordinate body, initial state 0 and owned, zeroed, counted
/// `EffectWork` in `spawnArg2.pointer`. Attachment discards the spawn offset and
/// follows the work's borrowed parent at its origin; that hierarchy must remain
/// live. `spawnArg1.value` uses `EFFECT_GLUTTON_RAIN_BLOB_*` from
/// `gameplay/glutton_rain_effect.h`; the projectile owner requests BURST on landing.
/// FLYING advances age twice per running tick and cycles eight billboard cells
/// using age / 2. Suspension redraws without advancing; cancellation redraws
/// before teardown. END releases work and task on the next running tick. Emitted
/// particles are independent; this overlay and rendering resources must stay loaded.
void shelterB3DumpingHoleGluttonRainBlobTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B3_DUMPING_HOLE_H
