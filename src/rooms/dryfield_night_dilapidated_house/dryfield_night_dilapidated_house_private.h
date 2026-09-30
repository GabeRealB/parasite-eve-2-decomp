#ifndef SRC_ROOMS_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_PRIVATE_H

#include "common.h"

#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"

#include "main/task_types.h"

// Retained exporter slots follow the active spotlights. Their contents
// include stale/incomplete addresses; preserve them as bytes pending review.
typedef struct {
    WorldCoordSpotLight active[1];
    u8                  retained[756];
} DryfieldNightDilapidatedHouseSpotLightStorage;
STATIC_ASSERT_SIZEOF(DryfieldNightDilapidatedHouseSpotLightStorage, 864);

/// The room's two-entry task descriptor table: entry 0 starts the streamed
/// sequence, entry 1 is the task that plays it.
extern TaskDesc D_dryfield_night_dilapidated_house_801872B4[];

extern GpRoomCoordSet D_dryfield_night_dilapidated_house_80189B60[1];

extern GpObj4C D_dryfield_night_dilapidated_house_80189B78[12];

extern GpObj3A D_dryfield_night_dilapidated_house_80189F08[1];

extern WorldCoordRoomAmbientEntry D_dryfield_night_dilapidated_house_8018A054[12];

extern TaskDesc gRoomEventTaskDesc;

extern GpMsgEntry D_dryfield_night_dilapidated_house_8017E700[5];

extern GpEvsCmd D_dryfield_night_dilapidated_house_801868F4[88];

extern GpEvsCmd D_dryfield_night_dilapidated_house_80187134[16];

extern GpPointLight D_dryfield_night_dilapidated_house_80189500[8];

extern DryfieldNightDilapidatedHouseSpotLightStorage D_dryfield_night_dilapidated_house_80189800;

// Callbacks referenced by the overlay's shared data tables.

s32 func_dryfield_night_dilapidated_house_8017D8D4(Task*, s32, TaskMessageArg, TaskMessageArg);

s32 func_dryfield_night_dilapidated_house_8017D8DC(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_dryfield_night_dilapidated_house_8017D960(Task*, s32, TaskMessageArg, TaskMessageArg);

s32 func_dryfield_night_dilapidated_house_8017D968(Task*, s32, TaskMessageArg, TaskMessageArg);

void func_dryfield_night_dilapidated_house_8017DA70(void);

void func_dryfield_night_dilapidated_house_8017DA90(void);

void func_dryfield_night_dilapidated_house_8017DAB0(void);

void func_dryfield_night_dilapidated_house_8017DAD0(void);

void func_dryfield_night_dilapidated_house_8017DAF0(void);

#endif // SRC_ROOMS_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_PRIVATE_H
