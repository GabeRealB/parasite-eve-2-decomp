#ifndef ROOMS_DRYFIELD_GAS_STATION_H
#define ROOMS_DRYFIELD_GAS_STATION_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgs.h>

#include "main/task.h"

/// Work block for the gas-station cutscene task, allocated as 0x10 zeroed bytes
/// by `func_dryfield_gas_station_801807E0` and hung off `Task::idMap` (0x1C).
///
/// `owner` is the slot-3 game pointer (`Game_GetPtrSlot(3)`) the task dispatches
/// its messages to, and `playerEffActive` is the flag guarding
/// `Gp_KillPlayerEffs` / `Gp_SpawnWeaponEff`. The two shorts at 0x4 are script
/// parameters written together by `func_dryfield_gas_station_80180B2C`; their
/// meaning is not yet known.
typedef struct DgsWork {
    /* 0x00 */ void* owner;
    /* 0x04 */ s16   field_4;
    /* 0x06 */ s16   field_6;
    /* 0x08 */ byte  pad_8[0x4];
    /* 0x0C */ u16   playerEffActive;
    /* 0x0E */ byte  pad_E[0x2];
} DgsWork;
STATIC_ASSERT_SIZEOF(DgsWork, 0x10);

/// Work block the gas station's shaft sequencer (`func_dryfield_gas_station_801802C0`)
/// allocates as 4 bytes in its state 0 and hangs off `Task::idMap` (0x1C) for
/// the next run of the state machine to pick up. `child` is the task spawned
/// from `D_dryfield_gas_station_8018312C` entry 0 in state 3 and polled with
/// `Task_PollKill` in state 4.
typedef struct DgsCutsceneSlot {
    /* 0x0 */ Task* child;
} DgsCutsceneSlot;
STATIC_ASSERT_SIZEOF(DgsCutsceneSlot, 0x4);

/// Draws the gas station's shaft from the same four arguments `Room_Draw37`
/// takes: `arg0` is updated with `Gp_UpdateCoord` and the beam is `arg1`
/// rotated by its `workm`. Larger and textured where `Room_Draw37` is not --
/// its body is still `INCLUDE_ASM`, so the rest is unverified.
void func_dryfield_gas_station_80181058(GsCOORDINATE2* arg0, SVECTOR* arg1, s32 arg2, s32 arg3);

/// `Task::spawnArg2` of the cap (cutscene) task this room family spawns.
/// `field_0` is the area id forced for the duration of the scene (negative =
/// keep the current one); `field_1` selects the cap slot / command;
/// `field_2` skips straight to the abort state; `field_3` is the cap file to
/// load. The four s32s are sound-event ids. `func_dryfield_gas_station_8017FD54`
/// writes `field_0` .. `field_10`; the trailing `field_14` / `field_16` pair is
/// inferred from the identically laid out `Shelter1fTentCapScript` and the
/// object's 0x18-byte extent, not from a matched body here.
typedef struct {
    /* 0x00 */ s8  field_0;
    /* 0x01 */ s8  field_1;
    /* 0x02 */ s8  field_2;
    /* 0x03 */ s8  field_3;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
    /* 0x14 */ s16 field_14;
    /* 0x16 */ s16 field_16;
} DryfieldGasStationCapScript;

STATIC_ASSERT_SIZEOF(DryfieldGasStationCapScript, 0x18);

extern DryfieldGasStationCapScript D_dryfield_gas_station_80184BD8;

#endif // ROOMS_DRYFIELD_GAS_STATION_H
