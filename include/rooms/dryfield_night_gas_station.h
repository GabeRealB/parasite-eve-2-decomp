#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_GAS_STATION_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_GAS_STATION_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_night_gas_station_80190624[12];

// dryfield_night_gas_station
extern WorldCoordRoomLighting D_dryfield_night_gas_station_80189DB0[];

extern GpRoomObjRec D_dryfield_night_gas_station_80189DD0[];

extern u8* D_dryfield_night_gas_station_80189E70[];

extern GpViewCountRec D_dryfield_night_gas_station_80189E80[];

extern GpWarpRec D_dryfield_night_gas_station_80189E88[];

extern GpViewRec D_dryfield_night_gas_station_8018B780[];

extern GpSprtRec D_dryfield_night_gas_station_8018F6C4[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_gas_station_80190780[];

void func_dryfield_night_gas_station_80181D80(Task* task);

void func_dryfield_night_gas_station_801827E4(Task* task);

void func_dryfield_night_gas_station_801830CC(Task* task);

void func_dryfield_night_gas_station_80180E9C(Task* task);

void func_dryfield_night_gas_station_8017E9F8(Task* task);

/// Task entries the Dryfield-at-night map UI overlay's stage tables name, each
/// room's entry task started for its location, and the models its enemy
/// descriptors attach.
void func_dryfield_night_gas_station_8017FB70(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_GAS_STATION_H
