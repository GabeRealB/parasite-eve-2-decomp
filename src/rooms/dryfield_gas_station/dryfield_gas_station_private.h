#ifndef SRC_ROOMS_DRYFIELD_GAS_STATION_DRYFIELD_GAS_STATION_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_GAS_STATION_DRYFIELD_GAS_STATION_PRIVATE_H

#include "main/task_types.h"

#include "rooms/room.h"

/// Descriptors of the tasks the gas station's sequencer spawns: entry 0 is the
/// cutscene task the sequencer waits on, entry 1 the fade the cutscene's last
/// script command starts.
extern TaskDesc D_dryfield_gas_station_8018312C[];

extern TaskDesc D_dryfield_gas_station_80181E7C[3];

extern Task* D_dryfield_gas_station_80184BCC;

extern Task* D_dryfield_gas_station_80184BD0;

extern RoomCutsceneRec D_dryfield_gas_station_80184BD8;

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_gas_station_8017FFE4(Task*);

void func_dryfield_gas_station_801801E4(Task*);

void func_dryfield_gas_station_801802C0(Task*);

#endif // SRC_ROOMS_DRYFIELD_GAS_STATION_DRYFIELD_GAS_STATION_PRIVATE_H
