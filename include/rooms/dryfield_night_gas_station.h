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
/// `spawnArg2.pointer` from `Gp_SpawnEff`. `spawnArg1.value` is a positive
/// charge duration in active ticks, consumed as a countdown. Nonzero room
/// effect control pauses it; values at least four cancel it. State 3 also
/// releases the work and task. Requires a live effect controller and room
/// overlay until teardown.
void dryfieldNightGasStationRoomVisualEffectsFlashTask(Task* task);

/// Runs the night gas station's fading sword beam between two moving endpoint histories.
///
/// Starts in state 0 with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `Gp_SpawnEff`; its parent coordinate must stay live.
/// Owns two eight-coordinate histories in `Task::work`, freed by task teardown.
/// Allocation failure retries with age zero. Initialization counts as the first
/// active tick; later ticks record endpoints and draw seven quads tinted 1:2:3
/// in R:G:B. `spawnArg1.value` is 0 for external teardown or 2..32767 for an
/// active-age limit. Room effect control at two or above suspends recording and
/// drawing without cancelling. Requires a live effect controller and room overlay.
void dryfieldNightGasStationRoomVisualEffectsTwinTrailTask(Task* task);

void func_dryfield_night_gas_station_801830CC(Task* task);

void func_dryfield_night_gas_station_80180E9C(Task* task);

void func_dryfield_night_gas_station_8017E9F8(Task* task);

/// Task entries the Dryfield-at-night map UI overlay's stage tables name, each
/// room's entry task started for its location, and the models its enemy
/// descriptors attach.
void func_dryfield_night_gas_station_8017FB70(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_GAS_STATION_H
