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

void neoArkPavilionWaterRippleTaskFixedCoord(Task* task);

void neoArkPavilionWaterDriftTaskU16FixedCoord(Task* task);

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

void func_neo_ark_pavilion_8017EBF4(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_PAVILION_H
