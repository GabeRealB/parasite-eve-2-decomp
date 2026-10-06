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

void neoArkIslandWaterRippleTaskFixedCoord(Task* task);

void neoArkIslandWaterDriftTaskU16FixedCoord(Task* task);

/// Runs the island's charging pink flash, peak screen tint and fading star.
///
/// Requires initial state 0, a coordinate body and a counted `EffectWork` in
/// `spawnArg2.pointer`, as supplied by `Gp_SpawnEff`. `spawnArg1.value` starts
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
/// `Gp_SpawnEff`. The borrowed `EffectWork::parent` and overlay must stay live.
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
