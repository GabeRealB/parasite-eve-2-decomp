#ifndef SRC_ROOMS_NEO_ARK_POWER_PLANT_1_NEO_ARK_POWER_PLANT_1_PRIVATE_H
#define SRC_ROOMS_NEO_ARK_POWER_PLANT_1_NEO_ARK_POWER_PLANT_1_PRIVATE_H

#include "types.h"

#include "gameplay/evs.h"
#include "gameplay/message.h"

#include "main/task_types.h"

extern GpMsgEntry D_neo_ark_power_plant_1_8017EB18[5];

extern GpEvsCmd D_neo_ark_power_plant_1_8017EB7C[24];

extern GpEvsCmd D_neo_ark_power_plant_1_8017EDBC[10];

extern GpEvsCmd D_neo_ark_power_plant_1_8017EEE4[13];

extern s32 D_neo_ark_power_plant_1_8017F01C;

extern s32 D_neo_ark_power_plant_1_80181B9C[3];

extern s32 D_neo_ark_power_plant_1_80181BA8[3];

// Callbacks referenced by the overlay's shared data tables.
s32 func_neo_ark_power_plant_1_8017D7AC(Task*, s32, GpMessageArg, GpMessageArg);

s32 func_neo_ark_power_plant_1_8017D7B4(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_neo_ark_power_plant_1_8017D7F8(Task*, s32, s32, s32);

s32 func_neo_ark_power_plant_1_8017D8C8(Task*, s32, GpMessageArg, GpMessageArg);

void func_neo_ark_power_plant_1_8017D8D0(void);

void func_neo_ark_power_plant_1_8017D908(void);

#endif // SRC_ROOMS_NEO_ARK_POWER_PLANT_1_NEO_ARK_POWER_PLANT_1_PRIVATE_H
