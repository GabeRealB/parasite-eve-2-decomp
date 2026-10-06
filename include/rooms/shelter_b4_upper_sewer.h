#ifndef INCLUDE_ROOMS_SHELTER_B4_UPPER_SEWER_H
#define INCLUDE_ROOMS_SHELTER_B4_UPPER_SEWER_H

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

// Room data exported to the shared waypoint actor.
extern SVECTOR* D_shelter_b4_upper_sewer_801866F8[4];

extern SVECTOR D_shelter_b4_upper_sewer_80186708[9];

extern s16 D_shelter_b4_upper_sewer_80186438;

extern AreaVariant D_shelter_b4_upper_sewer_80188B9C[12];

// shelter_b4_upper_sewer
extern u8* D_shelter_b4_upper_sewer_80186590[];

extern ViewCount D_shelter_b4_upper_sewer_80186594[];

extern DirectionWarpEntry D_shelter_b4_upper_sewer_80186598[];

extern WorldCollisionGrid D_shelter_b4_upper_sewer_80186EF8;

extern ViewCamera D_shelter_b4_upper_sewer_80186F1C[];

extern SpriteView D_shelter_b4_upper_sewer_801879BC[];

extern WorldCoordRoomLights D_shelter_b4_upper_sewer_80188184;

extern WorldCollisionTrigger D_shelter_b4_upper_sewer_8018819C[];

extern WorldCollisionTrigger D_shelter_b4_upper_sewer_801886F4[];

extern WorldCollisionOccluder D_shelter_b4_upper_sewer_80188BFC[];

extern WorldCollisionSurfaceProperties* D_shelter_b4_upper_sewer_80188CFC[];

void func_shelter_b4_upper_sewer_8017DC30(Task* task);

/// Runs the sewer's charging pink flash, peak screen tint and fading star.
///
/// Needs a counted effect with a coordinate body and owned `EffectWork` in
/// `spawnArg2.pointer`, starting in state 0. `spawnArg1.value` is a positive
/// charge duration in running ticks, consumed as a countdown. Nonzero room
/// effect control pauses; four or above cancels. Completion frees work and task.
void shelterB4UpperSewerRoomVisualEffectsFlashTask(Task* task);

/// Records and draws the sewer's two-ended trail from eight world-space snapshots.
///
/// Needs a counted effect, coordinate body, owned `EffectWork` in
/// `spawnArg2.pointer`, initial state 0 and NULL `task->work`. The work's
/// parent must stay live. Initialization allocates the two histories and
/// retries on failure. `spawnArg1.value` is the nonzero age at which to retire;
/// zero gives no age limit. Control values below `ROOM_EFFECT_CONTROL_HIDDEN`
/// advance the trail; other values stop it. Teardown frees work and histories.
void shelterB4UpperSewerRoomVisualEffectsTwinTrailTask(Task* task);

void func_shelter_b4_upper_sewer_80183A80(Task* task);

/// Installs the sewer's effect IDs once and draws glow capsules for the mapped view.
///
/// Start in state 0 with this room loaded. Ripple and spray IDs are installed
/// only when `GAME_FLAG_B4_RESERVOIR_EVENT_DONE` is exactly 1. Other IDs are
/// always installed. Subsequent ticks draw without consulting effect control;
/// this task does not retire itself or use its coordinate body or spawn arguments.
void shelterB4UpperSewerDrawGlowsTask(Task* task);

/// Grows and fades one ripple on the sewer's water surface.
///
/// Needs a counted effect with a coordinate body, owned `EffectWork` in
/// `spawnArg2.pointer` and initial state 0. `spawnArg1` bits 0..11 give the
/// initial local half-side in world units; higher bits are ignored. Running
/// ticks grow it by 32 and fade brightness from 64 by 2, for 32 draws.
/// Paused ticks redraw; cancellation redraws once before freeing work and task.
void shelterB4UpperSewerWaterRippleTask(Task* task);

/// Animates one sewer water-spray particle, with optional movement and gravity.
///
/// Needs a counted effect, coordinate body, owned zero-initialized `EffectWork`
/// in `spawnArg2.pointer` and initial state 0. `spawnArg1` packs size in bits
/// 0..11, ticks per cell in 12..15 (0 means 1), speed in 16..23 (0 means 64),
/// velocity kind in 24..27 (0 stationary, 1 upward burst, 2 spray, 3 narrow jet,
/// 5 direction from work's `pos`), and upright mode in 28..31 (nonzero selects
/// a tile). Supplied nonzero `move` bypasses velocity generation. Movement
/// uses parent-space units per running tick; gravity adds 6 to Y velocity.
/// Paused ticks redraw; cancellation or eight-cell completion frees work and task.
void shelterB4UpperSewerWaterDriftTask(Task* task);

void func_shelter_b4_upper_sewer_801846C8(Task* arg0);

/// Flies the sewer's animated spark along a step fixed from its initial target offset.
///
/// Needs a counted effect, coordinate body and owned `EffectWork` in
/// `spawnArg2.pointer`, starting in state 0. `spawnArg1.pointer` borrows a target
/// `GfxCoord`; both world matrices must be composed for initialization. The
/// target is sampled only then, deriving a parent-space step of 204/4096 of
/// the offset. Nonzero effect control pauses, four or above cancels, and age
/// 20 frees work and task. The target pointer must stay valid through initialization.
void shelterB4UpperSewerRoomVisualEffectsFlyingSparkTask(Task* task);

/// Draws the sewer's flying-section orange disc, layered glow and fading ring.
///
/// Needs a counted effect, coordinate body and owned `EffectWork` in
/// `spawnArg2.pointer`, starting in state 0. Spawn argument 1 is unused.
/// Running ticks expand the glow and ring, fade the ring, then fade the centre.
/// Nonzero effect control pauses; four or above cancels. Completion frees work and task.
void shelterB4UpperSewerRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

/// Moves and fades one animated sewer mote along its coordinate's local Y axis.
///
/// Needs a counted effect, coordinate body and owned zero-initialized
/// `EffectWork` in `spawnArg2.pointer`, starting in state 0. `spawnArg1` bits
/// 0..11 give world-unit half-extent, with overlapping bits 0..1 selecting
/// steady motion (bit 1 upward); 12..15 select the palette, 16..23 unsigned
/// speed, and 24..31 signed lifetime in running ticks. Initialization does
/// not draw. Nonzero effect control pauses; four or above cancels.
/// Completion frees work and task after the brightness fades to zero.
void shelterB4UpperSewerRoomVisualEffectsMoteTask(Task* task);

/// Expands the sewer's tinted halo and shrinking ring, then fades a star.
///
/// Needs a counted effect, coordinate body and owned zero-initialized
/// `EffectWork` in `spawnArg2.pointer`, starting in state 0. The work's parent
/// must stay live; `pos` is a parent-space offset in world units. Signed
/// `spawnArg1` halves give positive expansion ticks (low) and tint row 0..2
/// (high). The argument becomes a countdown. Nonzero effect control pauses;
/// four or above cancels. Completion frees work and task.
void shelterB4UpperSewerRoomVisualEffectsHaloTask(Task* task);

/// Draws the sewer's halo-section orange disc, layered glow and fading ring.
///
/// Needs a counted effect, coordinate body and owned `EffectWork` in
/// `spawnArg2.pointer`, starting in state 0. Spawn argument 1 is unused.
/// Running ticks expand the glow and ring, fade the ring, then fade the centre.
/// Nonzero effect control pauses; four or above cancels. Completion frees work and task.
void shelterB4UpperSewerRoomVisualEffectsHaloOrangeBurstTask(Task* task);

void func_shelter_b4_upper_sewer_80182600(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B4_UPPER_SEWER_H
