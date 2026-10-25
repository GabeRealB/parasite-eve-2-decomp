#ifndef INCLUDE_ROOMS_NEO_ARK_WOODLAND_PATH_H
#define INCLUDE_ROOMS_NEO_ARK_WOODLAND_PATH_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Spawn recipe for the woodland path's water-plane refraction task.
///
/// Creates a no-body task at priority `TASK_PRIORITY_CONTROL` with zero descriptor metadata.
/// Gameplay starts it on room entry; it draws water in logical views 6 and 7
/// at world Y=140. The room overlay must stay loaded while the task runs.
extern TaskDesc gNeoArkWoodlandPathWaterRefractionTaskDesc;

/// Number of retained placement-variant slots, including absent slots.
enum { NEO_ARK_WOODLAND_PATH_AREA_VARIANT_COUNT = 12 };

/// Neo Ark woodland path placement/resource directory indexed by `GameLocationKey.variant`.
///
/// Variants 1..3 each place two Horned Strangers; variant 4 places four.
/// All other slots, including 0, are absent (NULL, NULL).
/// Placements belong to the Neo Ark map overlay; recipes belong to this room.
/// Keep both and any actors supplying task descriptors loaded through loading,
/// spawning and model-flag updates.
/// Selectors must be in 0..11; there is no terminator or bounds check.
extern AreaVariant gNeoArkWoodlandPathAreaVariants[NEO_ARK_WOODLAND_PATH_AREA_VARIANT_COUNT];

/// Logical-view map for the woodland path's single room.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1..10 select the same
/// 1-based camera, image and sprite indices. The room directory has no terminator
/// and remains valid while this overlay is loaded.
extern u8* gNeoArkWoodlandPathViewMaps[1];

/// Logical-view reverse-search count for the woodland path's single room.
///
/// Room 1 searches ten entries of its identity map, independently of that
/// shared map's 50-byte capacity. The count array has no terminator and is
/// borrowed by the stage directory while this room overlay is loaded.
extern ViewCount gNeoArkWoodlandPathViewCounts[1];

/// Player and companion placements and sound cues for the path's two warps.
///
/// Indexed by warp ID minus one, with no terminator. Slots 1 and 2 arrive in
/// logical views 2 and 10 respectively; yaw uses 4096 units per turn and XYZ
/// uses room-coordinate units. Neither endpoint sets a map marker or special
/// departure option. The stage directory borrows this loaded room's storage.
extern DirectionWarpEntry gNeoArkWoodlandPathWarpEntries[2];

/// Woodland path collision mesh and its 4-by-5 XZ cell index.
///
/// The room owns 35 faces, 75 vertices, 17 normals and the cell lists. Cells
/// have 4000-unit sides, with X/Z origins at -7600/-7000 in room coordinates.
/// Activation binds the view transform; all mesh storage must remain loaded.
extern WorldCollisionGrid gNeoArkWoodlandPathCollisionGrid;

/// Camera transforms and projection distances for woodland logical views 1..10.
///
/// The identity map selects entry view minus one, without a terminator.
/// Rotation uses Q12 coefficients and translation is the negated world origin;
/// projection distances are 329 pixels for view 1 and 230 for views 2..10.
/// Camera application borrows the selected record from this loaded overlay.
extern ViewCamera gNeoArkWoodlandPathViewCameras[10];

/// Background-sprite resources for woodland logical views 1..10.
///
/// The identity map selects entry view minus one. Every view retains decoded
/// background strips and has no draw-area override; view 1 selects no sprites.
/// Source and batch lists are room-owned and must remain loaded during packet
/// allocation and drawing. The directory has no terminator.
extern SpriteView gNeoArkWoodlandPathSpriteViews[10];

/// Woodland path model lighting from seven point sources in every view.
///
/// Six white Q12 sources fade between radii 500 and 3000 world units; the
/// seventh stores equal inner/outer radius settings of 100000. There are no
/// directional or cone sources. Coordinate caches and attenuation are writable
/// and live with the room overlay.
extern WorldCoordRoomLights gNeoArkWoodlandPathRoomLights;

/// Directed collision boundaries between adjacent woodland views 2..10.
///
/// Eight boundary pairs carry source/destination view IDs in parameter0/1;
/// the final record includes WORLD_COLLISION_TRIGGER_LAST. Activation binds
/// and enables all sixteen records in the current view. Geometry uses room
/// units and Q12 normals; mutable links and hit latches live with the overlay.
extern WorldCollisionTrigger gNeoArkWoodlandPathViewBoundaryTriggers[16];

/// Two exit warps and six automatic roamer-spawn requests along the path.
///
/// The facing-gated warps lead to the forest zone and pyramid, using departure
/// slots 1 and 2 and arrival slots 3 and 1 respectively. Automatic quads send
/// room-action IDs 1..6 with argument zero to select pool-A spawn points.
/// Activation binds and enables the mutable records; LAST marks the included
/// eighth record. The overlay must remain loaded until the list is cleared.
extern WorldCollisionTrigger gNeoArkWoodlandPathActionTriggers[8];

/// Minimum ambient RGB levels for woodland path logical views 1..10.
///
/// Entry zero holds the view count; entries 1..10 hold signed levels in units
/// of 1/16 of an 8-bit colour level, plus retained brightness. Lighting borrows
/// the table only while the room overlay is loaded.
extern WorldCoordRoomAmbientEntry gNeoArkWoodlandPathRoomAmbientByView[11];

/// Four woodland quads that obstruct enemy line-of-sight queries.
///
/// Activation links and enables all records through the fourth one's LAST
/// marker. Origins and corner offsets are in room units, transformed through
/// the current view for sight tests; normals use Q12. Mutable list links remain
/// borrowed from this overlay until the active occluder list is cleared.
extern WorldCollisionOccluder gNeoArkWoodlandPathSightOccluders[4];

/// Woodland path collision and footstep policies indexed by surface class 0..7.
///
/// Every class blocks probes, permits weapon impacts and applies pushback.
/// Classes 2..4 supply footstep cues; classes 0, 1 and 5..7 have none. Slots
/// 5..7 reuse class 0's record. The directory and records live with this overlay.
extern WorldCollisionSurfaceProperties* gNeoArkWoodlandPathSurfaceProperties[8];

/// Advances and draws an expanding, fading water-surface ripple.
///
/// Requires a counted effect task with a coordinate body, initial state 0
/// and a cleared, owned primary-heap `EffectWork` in `spawnArg2.pointer`.
/// `spawnArg1` bits 0..11 give the initial half-side in coordinate units;
/// higher bits are ignored. Each running update grows it by 32 and dims the
/// brightness by 2, from 64. Suspended updates redraw the retained ripple.
/// Cancellation draws once before retirement; cancellation or the end of
/// the fade releases the work, coordinate body and task.
void neoArkWoodlandPathWaterRippleTask(Task* task);

/// Emits water ripples and spray from player movement on the woodland path.
///
/// Requires a live counted effect task with owned `EffectWork` in `spawnArg2`,
/// initial state 0, and a player model with initialized coordinate caches for
/// parts 15 and 18. The first tick selects this room's water effects and records
/// those parts' cached XYZ narrowed to signed halfwords.
///
/// Every tick enables the room effect view mode only while root Y is below 17.
/// While effect control is running and root Y is at least 300 (Y grows downward),
/// each part's summed absolute cached movement supplies spray odds out of 512;
/// ripples add 32 to those odds. The stored odds narrow to signed 16 bits before
/// the two rolls. Effects spawn at the root X/Z with parent-space Y=200: ripple
/// half-side 64, spray size 384, two updates per cell and launch speed 32.
/// Position snapshots advance only during emitting ticks. Spawning copies the
/// stack placement; these child effects do not follow its retained address.
/// The room must own the shared history and effect IDs for the task's lifetime;
/// this callback neither retires itself nor releases its work.
void neoArkWoodlandPathPlayerWaterSplashTask(Task* task);

/// Advances and draws an eight-cell water-spray particle with unsigned cell indices.
///
/// Requires a counted effect task with a coordinate body, initial state 0
/// and a cleared, owned primary-heap `EffectWork` in `spawnArg2.pointer`.
/// `spawnArg1` bits 0..11 give perspective size, bits 12..15 updates per
/// cell (0 selects 1), and bits 16..23 launch speed in parent-coordinate
/// units per running update (0 selects 64). Bits 24..27 select the launch:
/// 0 stationary, 1 upward burst, 2 all-axis spray, 3 narrow upward jet,
/// 5 the copied spawn-offset direction; other kinds retain a zero direction.
/// Nonzero bits 28..31 select upright drawing instead of a random fixed spin.
/// A supplied nonzero `EffectWork::move` bypasses launch generation and scaling.
/// The first running update initializes; later updates draw cells 0..7,
/// move in parent space and apply gravity unless stationary. Suspended
/// updates redraw; cancellation or finishing cell 7 releases the work,
/// coordinate body and task. No work pointer remains live after retirement.
void neoArkWoodlandPathWaterDriftTaskU16(Task* task);

/// Advances and draws a tumbling leaf through its fall, hold and fade.
///
/// Requires a counted effect task with a coordinate body, initial state 0
/// and a cleared, owned primary-heap `EffectWork` in `spawnArg2.pointer`.
/// Motion uses parent-coordinate units per tick and 4096-unit-turn angles.
/// The leaf has half-side 32 and stops moving once parent-space Y is positive.
/// Updates continue through room-effect suspension. The final fade releases
/// the work, coordinate body and task, ending their lifetimes.
void neoArkWoodlandPathLeafFallTask(Task* task);

/// Runs the woodland path room controller through setup, idle and teardown.
///
/// Requires a live no-body task whose state indexes 0..2, initially 0. Setup
/// installs the room messages, publishes `GAME_TASK_SLOT_ROOM` and spawns pool A.
/// State 1 preserves the live controller for synchronous messages; state 2 kills
/// it. The room overlay and its descriptor data must remain loaded through dispatch.
void neoArkWoodlandPathRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_WOODLAND_PATH_H
