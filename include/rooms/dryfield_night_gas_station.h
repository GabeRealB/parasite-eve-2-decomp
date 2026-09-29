#ifndef ROOMS_DRYFIELD_NIGHT_GAS_STATION_H
#define ROOMS_DRYFIELD_NIGHT_GAS_STATION_H

#include "gameplay/area.h"

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task_types.h"
#include "main/ui_types.h"

/// The block the room's effect task carries as its `spawnArg2`.
/// `func_dryfield_night_gas_station_80180E9C` keeps the spawn offset it hands
/// `Gp_SpawnEff` in `pos`, sets `active` once game flag nibble 0x63 has been
/// seen clear, and stores the per-anchor effect roll in `kind`. The bytes
/// around those fields are not reached here.
typedef struct DryfieldNightGasStationEffWork {
    byte    pad_0[0x10];
    SVECTOR pos;
    byte    pad_18[0xC];
    s16     active;
    s16     kind;
} DryfieldNightGasStationEffWork;

/// The room's task descriptor table; its spawners pick an entry by index.
extern TaskDesc D_dryfield_night_gas_station_801888A0[];

/// Handle of the task spawned from entry 0 of `D_dryfield_night_gas_station_801888A0`,
/// or NULL while none runs. `func_dryfield_night_gas_station_801807D4` either
/// passes it an argument or kills it.
extern Task* D_dryfield_night_gas_station_801907A4;

/// Handle of the task spawned from entry 1 or 3 of
/// `D_dryfield_night_gas_station_801888A0`, or NULL while none runs.
extern Task* D_dryfield_night_gas_station_801907A8;

/// UI descriptor of the help-line box (`func_dryfield_night_gas_station_8017ECF0`)
/// that the "Play Data" and usage panels open beside their lists.
extern UiObjectDesc D_dryfield_night_gas_station_80183FAC;

void func_dryfield_night_gas_station_80181D80(Task* task);
void func_dryfield_night_gas_station_801827E4(Task* task);
void func_dryfield_night_gas_station_801830CC(Task* task);
void func_dryfield_night_gas_station_80180E9C(Task* task);

void func_dryfield_night_gas_station_8017E9F8(Task* task);

extern GpAreaVariant D_dryfield_night_gas_station_80190624[12];

#endif // ROOMS_DRYFIELD_NIGHT_GAS_STATION_H
