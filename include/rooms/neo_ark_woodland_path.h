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

extern TaskDesc D_neo_ark_woodland_path_80181638;

extern AreaVariant D_neo_ark_woodland_path_8018471C[12];

// neo_ark_woodland_path
extern u8* D_neo_ark_woodland_path_80181694[];

extern ViewCount D_neo_ark_woodland_path_80181698[];

extern DirectionWarpEntry D_neo_ark_woodland_path_8018169C[];

extern WorldCollisionGrid D_neo_ark_woodland_path_80181D5C;

extern ViewCamera D_neo_ark_woodland_path_80181D80[];

extern SpriteView D_neo_ark_woodland_path_80183C6C[];

extern WorldCoordRoomLights D_neo_ark_woodland_path_80183F84;

extern WorldCollisionTrigger D_neo_ark_woodland_path_80183F9C[];

extern WorldCollisionTrigger D_neo_ark_woodland_path_8018445C[];

extern WorldCoordRoomAmbientEntry D_neo_ark_woodland_path_8018477C[];

extern WorldCollisionOccluder D_neo_ark_woodland_path_801847D4[];

extern WorldCollisionSurfaceProperties* D_neo_ark_woodland_path_80184910[];

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
