#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_GAS_STATION_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_GAS_STATION_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_gas_station_80190624[12];

// dryfield_night_gas_station
extern WorldCoordRoomLighting D_dryfield_night_gas_station_80189DB0[];

extern WorldCollisionRoomResources D_dryfield_night_gas_station_80189DD0[];

extern u8* D_dryfield_night_gas_station_80189E70[];

extern ViewCount D_dryfield_night_gas_station_80189E80[];

extern DirectionWarpEntry D_dryfield_night_gas_station_80189E88[];

extern ViewCamera D_dryfield_night_gas_station_8018B780[];

extern SpriteView D_dryfield_night_gas_station_8018F6C4[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_gas_station_80190780[];

/// Runs the night gas station's charging pink flash, peak screen tint and fading star.
///
/// Starts in state 0 with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`. `spawnArg1.value` is a positive
/// charge duration in active ticks, consumed as a countdown. Nonzero room
/// effect control pauses it; values at least four cancel it. State 3 also
/// releases the work and task. Requires a live effect controller and room
/// overlay until teardown.
void dryfieldNightGasStationRoomVisualEffectsFlashTask(Task* task);

/// Runs the night gas station's fading sword beam between two moving endpoint histories.
///
/// Starts in state 0 with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`; its parent coordinate must stay live.
/// Owns two eight-coordinate histories in `Task::work`, freed by task teardown.
/// Allocation failure retries with age zero. Initialization counts as the first
/// active tick; later ticks record endpoints and draw seven quads tinted 1:2:3
/// in R:G:B. `spawnArg1.value` is 0 for external teardown or 2..32767 for an
/// active-age limit. Room effect control at two or above suspends recording and
/// drawing without cancelling. Requires a live effect controller and room overlay.
void dryfieldNightGasStationRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs the night gas station's impact flash with smoke or orange rings and bouncing sparks.
///
/// Starts in state zero with a coordinate body and zero-aged, counted
/// `EffectWork` in `spawnArg2.pointer` from `effectSpawn`. Nonzero
/// `spawnArg1.value` selects smoke; zero selects the rings and sparks.
/// Ages 1..7 act, and age 8 releases the work and task. Room effect control
/// pauses at nonzero values below four and cancels at four or above. Child
/// effects own their lifetimes; the room overlay must remain loaded.
void dryfieldNightGasStationRoomVisualEffectsSparkBurstTask(Task* task);

/// Draws the room's view-selected light glows and scatters fire and smoke effects.
///
/// Requires a coordinate body and counted `EffectWork` in `spawnArg2.pointer`
/// from `effectSpawn`, retained until normal task teardown. State zero installs
/// the room's flash, twin-trail and spark-burst IDs and enables view effects;
/// this task does not advance its own state. The mapped view must be 0..20.
/// Capsule pairs and flares use world points; spawn offsets use the task's
/// coordinate frame, with independent X/Z jitter in -255..256 units.
///
/// Initial gas-station progress latches `EffectWork::scale`. Outside battle
/// and with running effect control, each scatter anchor chooses
/// `EFFECT_ADDITIVE_PUFF`, `EFFECT_FIRE_BURST`, or a further one-in-three
/// `EFFECT_SMOKE_PUFF`. Later progress emits smoke only, one-in-three per
/// anchor, and only if this task previously latched initial progress. Lights
/// continue drawing independently of these spawn gates.
void dryfieldNightGasStationAmbientEffectsTask(Task* task);

/// Updates the night gas station's telephone save and statistics menu.
///
/// The task owns a live `UiObject` in `spawnArg2.pointer` and starts in state
/// zero. A prior clear or attract-demo scene 1 enables statistics; otherwise
/// it opens the save dialog directly. Child panels supply the save result and
/// notices, while this callback publishes the menu result and UI-open state.
/// The room overlay and UI task tree must remain live while the menu runs.
void dryfieldNightGasStationTelephoneMenuTask(Task* task);

/// Task entries the Dryfield-at-night map UI overlay's stage tables name, each
/// room's entry task started for its location, and the models its enemy
/// descriptors attach.
/// Dispatches the night gas station's room receiver and deferred scene updates.
///
/// Requires a live bodyless task in state 0..2: initialize, update, release.
/// Restores saved balcony sprite visibility before every state call. The map
/// overlay and this room's callbacks and sprite tables must stay loaded. The
/// registered room-task slot borrows the task and is not cleared on teardown.
void dryfieldNightGasStationRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_GAS_STATION_H
