#ifndef INCLUDE_ROOMS_DRYFIELD_GAS_STATION_H
#define INCLUDE_ROOMS_DRYFIELD_GAS_STATION_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern GpAreaVariant D_dryfield_gas_station_80184A38[12];

// dryfield_gas_station
extern GpRoomObjRec D_dryfield_gas_station_8018314C[];

extern u8* D_dryfield_gas_station_8018315C[];

extern WorldCoordRoomLighting D_dryfield_gas_station_80183160[];

extern ViewCount D_dryfield_gas_station_80183168[];

extern GpWarpRec D_dryfield_gas_station_8018316C[];

extern ViewCamera D_dryfield_gas_station_80183EC8[];

extern SpriteView D_dryfield_gas_station_801842A8[];

extern WorldCollisionSurfaceProperties* D_dryfield_gas_station_80184BAC[];

void func_dryfield_gas_station_80181A78(Task* arg0);

void func_dryfield_gas_station_8017EA90(Task* task);

/// Task entries the Dryfield map UI overlay's stage tables name: each room's
/// entry task, started for its location, and the enemy descriptors' tasks.
void func_dryfield_gas_station_8017FF8C(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_GAS_STATION_H
