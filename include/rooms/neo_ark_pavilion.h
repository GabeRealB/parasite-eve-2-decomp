#ifndef INCLUDE_ROOMS_NEO_ARK_PAVILION_H
#define INCLUDE_ROOMS_NEO_ARK_PAVILION_H

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
extern SVECTOR* D_neo_ark_pavilion_80183A64[4];

extern SVECTOR D_neo_ark_pavilion_80183A74[8];

extern s16 D_neo_ark_pavilion_801839A0;

extern TaskDesc D_neo_ark_pavilion_8018384C;

extern AreaVariant D_neo_ark_pavilion_801876C4[13];

// neo_ark_pavilion
extern WorldCollisionRoomResources D_neo_ark_pavilion_801838B4[];

extern WorldCoordRoomLighting D_neo_ark_pavilion_801838D4[];

extern u8* D_neo_ark_pavilion_801838EC[];

extern ViewCount D_neo_ark_pavilion_801838F4[];

extern DirectionWarpEntry D_neo_ark_pavilion_801838F8[];

extern ViewCamera D_neo_ark_pavilion_80184208[];

extern SpriteView D_neo_ark_pavilion_801873B8[];

extern WorldCollisionSurfaceProperties* D_neo_ark_pavilion_801879EC[];

/// Installs the pavilion's eight enemy and water effect IDs on the first task tick.
///
/// State 0 installs the IDs and advances to state 1; later ticks are idle.
/// Spawn arguments and the coordinate body are unused. Keep the pavilion overlay
/// loaded while tasks using these IDs are live.
void neoArkPavilionInstallRoomEffectsTask(Task* task);

/// Advances an expanding, fading pavilion water ripple using its cached draw transform.
///
/// Requires a live counted effect with owned `EffectWork` in `spawnArg2.pointer`,
/// one coordinate body with a composed `workm`, initial state 0 and age 0,
/// as `effectSpawn` supplies. Spawn bits 0..11 give the initial local half-side
/// (0..4095 coordinate units); higher bits are ignored. Half-side is stored in
/// `EffectWork::angle` and RGB brightness in `EffectWork::scale`.
///
/// Each running update ages, grows by 32, draws, then dims by 2. The first sets
/// brightness 64 and replaces local rotation with one random Y rotation,
/// preserving translation. The task never composes: its first draw uses the
/// cache before that yaw, and model draw passes refresh the linked coordinate.
/// A fresh ripple lasts 32 running updates, drawing brightness 64..2 and
/// half-sides from initial size + 32 through initial size + 1024 (32..5119).
///
/// Non-running control freezes initialization and aging but still draws;
/// control 4 or above draws once before cancellation. Completion and cancellation
/// release the work, counted task and body. Keep the overlay loaded and scratch
/// and frame packet storage available while live; released pointers must not be retained.
void neoArkPavilionWaterRippleTask(Task* task);

/// Animates one eight-cell pavilion water-spray particle using its cached draw coordinate.
///
/// Requires a live counted effect with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body, initial state 0 and cell index 0, as `effectSpawn` supplies.
/// Drawers read the coordinate's composed `workm`; this task never composes it.
/// Movement changes local translation in parent-coordinate units and marks it
/// dirty, so drawing uses the cached position until another user composes it.
///
/// `spawnArg1` bits 0..11 give perspective size (0..4095), bits 12..15 running
/// updates per cell (0 selects 1), and bits 16..23 launch speed in parent units
/// per update (0 selects 64). Bits 24..27 select direction: 0 stationary,
/// 1 upward burst, 2 all-axis spray, 3 narrow upward jet, 5 the copied `pos`
/// offset; other values leave the zero direction for SDK normalization.
/// Nonzero bits 28..31 select the upright drawer; otherwise the sprite uses
/// a retained random angle in 4096 units per turn. A supplied nonzero `move`
/// bypasses direction generation and scaling; `step` becomes 64 to enable it.
///
/// The first running update initializes without drawing or moving. Later
/// updates draw cells 0..7 through u16 indices, move by signed halfword
/// velocity, and add 6 to Y velocity with halfword truncation. Stationary
/// particles skip movement and gravity. Each cell lasts `period` running
/// updates; retirement follows initialization plus 8 * `period` updates.
/// Non-running control freezes initialization and aging but still draws,
/// including once before cancellation at control 4 or above. Retirement frees
/// the work and tears down the counted task and body. Keep the overlay loaded
/// and scratch/frame packet storage available while live; released pointers
/// must not be retained.
void neoArkPavilionWaterSprayTask(Task* task);

/// Runs the pavilion's pink charge flash, peak screen tint and fading star.
///
/// Requires a coordinate body and an owned, zero-initialized `EffectWork` in
/// `spawnArg2.pointer`. `spawnArg1.value` is a positive charge duration in active
/// ticks, consumed as a countdown. Nonzero room effect control pauses the task;
/// control 4 or above cancels it. Completion and cancellation release the work.
void neoArkPavilionRoomVisualEffectsFlashTask(Task* task);

/// Records two anchored endpoints in eight-frame histories and draws their fading beam.
///
/// Requires a coordinate body and an owned, zero-initialized `EffectWork` in
/// `spawnArg2.pointer`; its `parent` coordinate must remain live. Owns a separate
/// history allocation in `Task::work`, released by task teardown. Allocation
/// failure resets the age and retries. After initialization, `spawnArg1.value`
/// stops the effect when it equals the nonzero signed 16-bit age; 0 disables
/// the timed stop. Control 0 and 1 record and draw; control 2 or above freezes
/// the history until resumed or torn down.
void neoArkPavilionRoomVisualEffectsTwinTrailTask(Task* task);

void func_neo_ark_pavilion_80180FFC(Task* task);

void func_neo_ark_pavilion_80181C44(Task* arg0);

/// Sends an animated spark toward an initial target position for twenty active ticks.
///
/// Requires a coordinate body and an owned, zero-initialized `EffectWork` in
/// `spawnArg2.pointer`. `spawnArg1.pointer` borrows a target `GfxCoord`; both
/// matrices must be current in the same view space on the first active tick.
/// The target is sampled once to fix the flight step. Nonzero room effect control pauses the
/// task; control 4 or above cancels it. Completion and cancellation release the work.
void neoArkPavilionRoomVisualEffectsFlyingSparkTask(Task* task);

/// Runs the pavilion's expanding orange disc and layered glow with a fading ring.
///
/// Requires a coordinate body and an owned, zero-initialized `EffectWork` in
/// `spawnArg2.pointer`; `spawnArg1` is unused. The ring fades before the centre.
/// Nonzero room effect control pauses the task; control 4 or above cancels it.
/// Completion and cancellation release the work.
void neoArkPavilionRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

/// Runs the pavilion's room-message task.
///
/// Requires a live task in state 0..2: initialize the receiver and ambience,
/// idle while messages handle requests, then teardown. The room overlay must
/// remain loaded through dispatch.
void neoArkPavilionRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_PAVILION_H
