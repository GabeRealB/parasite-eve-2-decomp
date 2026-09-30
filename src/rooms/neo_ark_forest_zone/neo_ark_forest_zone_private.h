#ifndef SRC_ROOMS_NEO_ARK_FOREST_ZONE_NEO_ARK_FOREST_ZONE_PRIVATE_H
#define SRC_ROOMS_NEO_ARK_FOREST_ZONE_NEO_ARK_FOREST_ZONE_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/evs.h"
#include "gameplay/message.h"

#include "main/task_types.h"

extern ActorCommand D_neo_ark_forest_zone_80182E44;

extern u16 D_neo_ark_forest_zone_80182E54[5];

extern TaskDesc D_neo_ark_forest_zone_80181DBC;

extern GpMsgEntry D_neo_ark_forest_zone_80181DC8[6];

extern s32 D_neo_ark_forest_zone_80181E30;

extern s32 D_neo_ark_forest_zone_80181E38;

extern Task* D_neo_ark_forest_zone_80181E68;

extern GpEvsCmd D_neo_ark_forest_zone_80181E6C[23];

extern TaskDesc D_neo_ark_forest_zone_80182E18;

// Callbacks referenced by the overlay's shared data tables.

s32 func_neo_ark_forest_zone_8017D7DC(Task*, s32, TaskMessageArg, TaskMessageArg);

s32 func_neo_ark_forest_zone_8017D7E4(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_neo_ark_forest_zone_8017D950(Task*, s32, TaskMessageArg, TaskMessageArg);

s32 func_neo_ark_forest_zone_8017D958(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_neo_ark_forest_zone_8017DA14(Task*, s32, s32, s32);

void func_neo_ark_forest_zone_8017DA48(void);

#endif // SRC_ROOMS_NEO_ARK_FOREST_ZONE_NEO_ARK_FOREST_ZONE_PRIVATE_H
