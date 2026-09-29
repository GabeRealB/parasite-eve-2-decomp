#ifndef NEO_ARK_FOREST_ZONE_PRIVATE_H
#define NEO_ARK_FOREST_ZONE_PRIVATE_H

#include "main/task_types.h"

#include "gameplay/message.h"

#include "gameplay/direction.h"

#include "common.h"

// Callbacks referenced by the overlay's shared data tables.
void func_neo_ark_forest_zone_8017D644(Task *);
s32 func_neo_ark_forest_zone_8017D7DC(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_neo_ark_forest_zone_8017D7E4(Task *, s32, GpSaveLoc *, GpSaveLoc *);
s32 func_neo_ark_forest_zone_8017D950(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_neo_ark_forest_zone_8017D958(Task *, s32, RoomEventMsg *, RoomEventMsg *);
s32 func_neo_ark_forest_zone_8017DA14(Task *, s32, s32, s32);
void func_neo_ark_forest_zone_8017DA48(void);

#endif // NEO_ARK_FOREST_ZONE_PRIVATE_H
