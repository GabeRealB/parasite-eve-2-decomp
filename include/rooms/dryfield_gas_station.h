#ifndef ROOMS_DRYFIELD_GAS_STATION_H
#define ROOMS_DRYFIELD_GAS_STATION_H

#include "common.h"

#include "main/task.h"

/// Descriptors of the tasks the gas station's sequencer spawns: entry 0 is the
/// cutscene task the sequencer waits on, entry 1 the fade the cutscene's last
/// script command starts.
extern TaskDesc D_dryfield_gas_station_8018312C[];

#endif // ROOMS_DRYFIELD_GAS_STATION_H
