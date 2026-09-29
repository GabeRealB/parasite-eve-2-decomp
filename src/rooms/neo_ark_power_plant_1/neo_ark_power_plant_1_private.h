#ifndef NEO_ARK_POWER_PLANT_1_PRIVATE_H
#define NEO_ARK_POWER_PLANT_1_PRIVATE_H

#include "main/task_types.h"

#include "gameplay/message.h"

#include "gameplay/direction.h"

#include "common.h"

// Callbacks referenced by the overlay's shared data tables.
s32 func_neo_ark_power_plant_1_8017D7AC(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_neo_ark_power_plant_1_8017D7B4(Task *, s32, GpSaveLoc *, GpSaveLoc *);
s32 func_neo_ark_power_plant_1_8017D7F8(Task *, s32, s32, s32);
s32 func_neo_ark_power_plant_1_8017D8C8(Task *, s32, GpMessageArg, GpMessageArg);
void func_neo_ark_power_plant_1_8017D8D0(void);
void func_neo_ark_power_plant_1_8017D908(void);

#endif // NEO_ARK_POWER_PLANT_1_PRIVATE_H
