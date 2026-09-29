#ifndef DRYFIELD_NIGHT_DILAPIDATED_HOUSE_PRIVATE_H
#define DRYFIELD_NIGHT_DILAPIDATED_HOUSE_PRIVATE_H

#include "main/task_types.h"

#include "gameplay/message.h"

#include "gameplay/light.h"

// Retained exporter slots follow the active spotlights. Their contents
// include stale/incomplete addresses; preserve them as bytes pending review.
typedef struct {
    GpSpotLight active[1];
    u8 retained[756];
} DryfieldNightDilapidatedHouseSpotLightStorage;
STATIC_ASSERT_SIZEOF(DryfieldNightDilapidatedHouseSpotLightStorage, 864);

extern DryfieldNightDilapidatedHouseSpotLightStorage D_dryfield_night_dilapidated_house_80189800;

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_night_dilapidated_house_8017D764(Task *);
s32 func_dryfield_night_dilapidated_house_8017D8D4(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_night_dilapidated_house_8017D8DC(Task *, s32, RoomEventMsg *, RoomEventMsg *);
s32 func_dryfield_night_dilapidated_house_8017D960(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_night_dilapidated_house_8017D968(Task *, s32, GpMessageArg, GpMessageArg);
void func_dryfield_night_dilapidated_house_8017DA70(void);
void func_dryfield_night_dilapidated_house_8017DA90(void);
void func_dryfield_night_dilapidated_house_8017DAB0(void);
void func_dryfield_night_dilapidated_house_8017DAD0(void);
void func_dryfield_night_dilapidated_house_8017DAF0(void);

#endif
