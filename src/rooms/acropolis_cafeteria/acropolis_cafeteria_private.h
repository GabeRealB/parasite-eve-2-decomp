#ifndef SRC_ROOMS_ACROPOLIS_CAFETERIA_ACROPOLIS_CAFETERIA_PRIVATE_H
#define SRC_ROOMS_ACROPOLIS_CAFETERIA_ACROPOLIS_CAFETERIA_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/area_flags.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"

#include "main/task_types.h"

// Retained exporter slots follow the active spotlights. Their contents
// include stale/incomplete addresses; preserve them as bytes pending review.
typedef struct {
    WorldCoordSpotLight active[1];
    u8                  retained[1512];
} AcropolisCafeteriaSpotLightStorage;
STATIC_ASSERT_SIZEOF(AcropolisCafeteriaSpotLightStorage, 1620);

extern TaskDesc D_acropolis_cafeteria_80184178[];

extern GpMsgEntry D_acropolis_cafeteria_80184CEC[2];

extern s32 D_acropolis_cafeteria_80184CFC;

extern WorldCoordPointLight D_acropolis_cafeteria_80189E24[15];

extern GpRoomCoordSet D_acropolis_cafeteria_8018AA18[1];

extern WorldCoordRoomAmbientEntry D_acropolis_cafeteria_8018C90C[25];

extern GpAreaApplyRec D_acropolis_cafeteria_8018C9D4[3];

extern s32 D_acropolis_cafeteria_8018D6A0;

extern s32 D_acropolis_cafeteria_8018D6A4;

extern s32 D_acropolis_cafeteria_8018D6A8;

extern AcropolisCafeteriaSpotLightStorage D_acropolis_cafeteria_8018A3C4;

// Callbacks referenced by the overlay's shared data tables.
void func_acropolis_cafeteria_8017E47C(Task*);

void func_acropolis_cafeteria_8017E658(Task*);

void func_acropolis_cafeteria_8017E6B8(Task*);

s32 func_acropolis_cafeteria_8017F908(Task*, s32, s32, s32);

#endif // SRC_ROOMS_ACROPOLIS_CAFETERIA_ACROPOLIS_CAFETERIA_PRIVATE_H
