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

// Room data exported to the shared waypoint actor.
extern SVECTOR* D_shelter_b4_reservoir_801851D4[4];

extern SVECTOR D_shelter_b4_reservoir_801851E4[5];

extern s16 D_shelter_b4_reservoir_80184F80;

extern AreaVariant D_shelter_b4_reservoir_80187350[12];

extern u8 D_shelter_b4_reservoir_801850C8[16];

extern u8 D_shelter_b4_reservoir_801850D8[16];

// shelter_b4_reservoir
extern WorldCoordRoomLighting D_shelter_b4_reservoir_801850E8[];

extern WorldCollisionRoomResources D_shelter_b4_reservoir_801850F8[];

extern u8* D_shelter_b4_reservoir_80185118[];

extern ViewCount D_shelter_b4_reservoir_80185120[];

extern DirectionWarpEntry D_shelter_b4_reservoir_80185124[];

extern ViewCamera D_shelter_b4_reservoir_80185ADC[];

extern SpriteView D_shelter_b4_reservoir_80186730[];

extern WorldCollisionSurfaceProperties* D_shelter_b4_reservoir_80187480[];

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
