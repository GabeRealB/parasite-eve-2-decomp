#ifndef INCLUDE_ROOMS_SHELTER_B4_UPPER_SEWER_H
#define INCLUDE_ROOMS_SHELTER_B4_UPPER_SEWER_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "rooms/room_visual_effects.h"

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

/// Dispatches the upper sewer's room setup, message-only idle state and teardown.
///
/// Start in state 0 with this room loaded. Setup installs the room's message
/// table and task slot, then advances to state 1. A nonzero reservoir-event flag
/// also starts water rendering at height -1800 world units; zero sets height 0.
/// State 2 kills the task. Only states 0..2 are valid; the task has no body and
/// ignores its packed spawn-location argument. The room must stay loaded.
void shelterB4UpperSewerRoomTask(Task* task);

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

/// Emits the sewer's impact flash with smoke puffs or sparks and fading rings.
///
/// Needs a counted effect with a coordinate body, owned zero-aged `EffectWork`
/// in `spawnArg2.pointer` and initial state 0. Nonzero `spawnArg1.value` selects
/// smoke; zero selects two independent bouncing sparks and two orange rings.
/// Running age 7 enters release; age 8 frees work and task. Nonzero effect
/// control pauses; four or above cancels. Spawned effects outlive this task.
void shelterB4UpperSewerRoomVisualEffectsSparkBurstTask(Task* task);

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

/// Runs the sewer's attached charge disc, player-joint sparks and fading release ring.
///
/// Needs a counted effect with a coordinate body, owned zeroed `EffectWork`
/// in `spawnArg2.pointer` and initial `ROOM_VISUAL_EFFECTS_GLOW_DISC_ATTACH` state.
/// `spawnArg1.value` selects tint 0 or 1. The work's parent coordinate and its
/// ancestors must stay live; `pos` is the offset in its axes, in world units. Growth
/// emits adopted flying sparks from player model parts 3..18 every fourth age,
/// requiring the player model and installed flying-spark callback to stay live.
/// The owner requests flicker, release or cancel through the glow-disc states.
/// Nonzero effect control pauses; four or above cancels. Release or cancellation
/// frees work, task and adopted sparks.
void shelterB4UpperSewerRoomVisualEffectsGlowDiscTask(Task* task);

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

/// Emits twenty motes at rotating, rising offsets around the sewer effect's coordinate.
///
/// Needs a counted effect with a coordinate body and owned zero-aged `EffectWork`
/// in `spawnArg2.pointer`; state and `spawnArg1` are unused. Running ages 1..20
/// emit independent motes at about 768 local radial units and Y = -128 * age.
/// Each mote descends eight parent-axis units per running tick. The room's mote
/// ID and callback must be installed and loaded. Age 21 frees work and task.
/// Nonzero effect control pauses; four or above cancels. Motes outlive the emitter.
void shelterB4UpperSewerRoomVisualEffectsSparkEmitterTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B4_UPPER_SEWER_H
