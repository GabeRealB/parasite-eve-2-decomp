#ifndef ROOMS_DRYFIELD_GAS_STATION_H
#define ROOMS_DRYFIELD_GAS_STATION_H

#include "gameplay/area.h"

#include "common.h"

#include "main/task_types.h"

/// Descriptors of the tasks the gas station's sequencer spawns: entry 0 is the
/// cutscene task the sequencer waits on, entry 1 the fade the cutscene's last
/// script command starts.
extern TaskDesc D_dryfield_gas_station_8018312C[];

void func_dryfield_gas_station_80181A78(Task* arg0);
extern GpAreaVariant D_dryfield_gas_station_80184A38[12];

void func_dryfield_gas_station_8017EA90(Task* task);

#endif // ROOMS_DRYFIELD_GAS_STATION_H
