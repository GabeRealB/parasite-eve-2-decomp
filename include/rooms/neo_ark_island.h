#ifndef INCLUDE_ROOMS_NEO_ARK_ISLAND_H
#define INCLUDE_ROOMS_NEO_ARK_ISLAND_H

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
extern SVECTOR* D_neo_ark_island_80181CE8[4];

extern SVECTOR D_neo_ark_island_80181CF8[7];

extern s16 D_neo_ark_island_80181C24;

extern TaskDesc D_neo_ark_island_80181B30;

extern AreaVariant D_neo_ark_island_80183F48[13];

// neo_ark_island
extern WorldCollisionRoomResources D_neo_ark_island_80181B94[];

extern WorldCoordRoomLighting D_neo_ark_island_80181BA4[];

extern u8* D_neo_ark_island_80181BAC[];

extern ViewCount D_neo_ark_island_80181BB0[];

extern DirectionWarpEntry D_neo_ark_island_80181BB4[];

extern ViewCamera D_neo_ark_island_801826EC[];

extern SpriteView D_neo_ark_island_80183B14[];

extern WorldCollisionSurfaceProperties* D_neo_ark_island_80183FE8[];

/// Installs the island's flash, twin-trail, spark-burst and water effect IDs once.
///
/// The room-effect controller spawns this counted coordinate-body task with
/// state 0. It publishes this overlay's effect IDs, then stays idle in state 1
/// until external teardown. Spawn arguments are unused; keep the room overlay
/// loaded while the task or the effects selected by these IDs are live.
void neoArkIslandInstallRoomEffectIdsTask(Task* task);

/// Advances an expanding, fading island water ripple using its cached draw transform.
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
void neoArkIslandWaterRippleTask(Task* task);

/// Animates one eight-cell island water-spray particle using its cached draw coordinate.
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
void neoArkIslandWaterSprayTask(Task* task);

/// Runs the island's charging pink flash, peak screen tint and fading star.
///
/// Requires initial state 0, a coordinate body and a counted `EffectWork` in
/// `spawnArg2.pointer`, as supplied by `effectSpawn`. `spawnArg1.value` starts
/// as a positive charge duration in running updates. The first update sets
/// the ramp; subsequent charge updates consume the argument as a countdown.
/// Nonzero room effect control pauses it; values 4 and above cancel it.
/// State 3 requests release on the next running update. Cancellation and
/// completion release the effect work, task and body. Keep the overlay loaded
/// and scratch/frame packet storage available while drawing.
void neoArkIslandRoomVisualEffectsFlashTask(Task* task);

/// Draws a fading blue ribbon between two offsets on the effect's moving parent.
///
/// Requires initial state 0, null `Task::work`, a coordinate body and counted
/// `EffectWork` with zero age in `spawnArg2.pointer`, as supplied by
/// `effectSpawn`. The borrowed `EffectWork::parent` and overlay must stay live.
/// Owns two eight-coordinate world-space histories in `Task::work`;
/// allocation failure resets age and retries on the next active update.
/// `spawnArg1.value` is the release age in active updates (2..32767 before
/// signed-halfword age wraps; 0 leaves lifetime to external teardown).
/// Initialization counts as the first update without drawing or testing expiry.
/// Room effect control below 2 advances it; values 2 and above freeze it
/// without drawing or cancellation. Teardown frees both histories, the effect
/// work and the body. Drawing requires scratch and frame packet storage.
void neoArkIslandRoomVisualEffectsTwinTrailTask(Task* task);

void func_neo_ark_island_80180EE8(Task* task);

void func_neo_ark_island_8017EB10(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_ISLAND_H
